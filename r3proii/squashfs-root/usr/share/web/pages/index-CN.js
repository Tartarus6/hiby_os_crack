/*
 Copyright (c) 2012-2015, Pierre-Olivier Latour
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * The name of Pierre-Olivier Latour may not be used to endorse
 or promote products derived from this software without specific
 prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL PIERRE-OLIVIER LATOUR BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


var ENTER_KEYCODE = 13;
// var MAX_CHUNK_SIZE = 2 * 1024 * 1024; //2MB

var _path = null;
var _pendingReloads = [];
var _reloadingDisabled = 0;

var _device = "";
var company = "HiBy";

$.ajax({
  url: '../hostname',
  dataType: 'text',
  success: function(data) {
    _device = company + ' ' + data.toUpperCase();
  }
});


//TODO: configs here
var _home = "/data/mnt/sd_0/";
var _root = "/data/mnt/";
var _homes = ["/data/mnt/sd_0/", "/data/mnt/sd_1/", "/data/mnt/udisk_0/", "/data/mnt/udisk_1/"];
// sync with index.html
var _validFileTypes = ["ISO","DFF","DSF","DTS","APE","FLAC","AIF","AIFF","WAV","M4A","M4B","AAC","MP2","MP3","OGG","OGA","WMA","CUE","M3U","M3U8","OPUS","BMP","PNG","JPG","JPEG","LRC","UPT","T","TXT"];

function _isHome(path) {
  for (var i = 0; i < _homes.length; i++) {
    if (path == _homes[i])
      return true;
  }
  return false;
}

function _isRoot(path) {
  return (_root == path);
}

function _isContainHome(path) {
  for (var i = 0; i < _homes.length; i++) {
    if (path.search(_homes[i]) >= 0)
      return true;
  }
  return false;
}

function formatFileSize(bytes) {
  if (bytes >= 0x40000000) {
    return (bytes / 0x40000000).toFixed(2) + ' GB';
  }
  if (bytes >= 0x100000) {
    return (bytes / 0x100000).toFixed(2) + ' MB';
  }
  return (bytes / 0x400).toFixed(2) + ' KB';
}

var _folderCache = {};       // 最终创建成功的目录缓存
var _pendingFolders = {};    // 正在创建中的目录记录
// ================ 标准化路径格式 ================
function _normalizePath(path) {
  return path.replace(/\/+/g, '/').replace(/\/$/, '') + '/';
}
// ================ 目录创建 ================
function createParentFolders(basePath, relativePath) {
  var folders = relativePath.split('/').filter(function(x) { return x; }).slice(0, -1);
  var currentPath = _normalizePath(basePath);
  
  return folders.reduce(function(chain, folder) {
    return chain.then(function() {
      currentPath = _normalizePath(currentPath + folder);
      
      // 已存在的目录直接跳过
      if (_folderCache[currentPath]) return Promise.resolve();
      
      // 正在创建的目录等待结果
      if (_pendingFolders[currentPath]) {
        return _pendingFolders[currentPath];
      }

      // 创建新目录
      var creation = new Promise(function(resolve, reject) {
        $.ajax({
          url: 'create',
          type: 'POST',
          data: { path: currentPath },
          dataType: 'json'
        }).done(function() {
          _folderCache[currentPath] = true;
          resolve();
        }).fail(function(jqXHR) {
          if (jqXHR.status === 409) { // 假设409为目录已存在
            _folderCache[currentPath] = true;
            resolve();
          } else {
            reject('目录创建失败: ' + currentPath);
          }
        }).always(function() {
          delete _pendingFolders[currentPath]; // 清理进行中记录
        });
      });

      _pendingFolders[currentPath] = creation; // 记录进行中状态
      return creation;
    });
  }, Promise.resolve());
}


function _isValidFileType(path)
{
  if(path.endsWith("/")) return true;
  
  var ext = path.substring(path.lastIndexOf(".")+1, path.length).toUpperCase();

  for (var i = 0; i < _validFileTypes.length; i++) {
    if (ext == _validFileTypes[i])
      return true;
  }
  return false;
}

function _showError(message, textStatus, errorThrown) {
  $("#alerts").html(tmpl("template-alert", {
    level: "danger",
    title: (errorThrown != "" ? errorThrown : textStatus) + ": ",
    description: message
  }));
}

function _showErrors(message, textStatus, errorThrown) {
  $("#alerts").prepend(tmpl("template-alert", {
    level: "danger",
    title: (errorThrown != "" ? errorThrown : textStatus) + ": ",
    description: message
  }));
}

function _disableReloads() {
  _reloadingDisabled += 1;
}

function _enableReloads() {
  _reloadingDisabled -= 1;

  if (_pendingReloads.length > 0) {
    _reload(_pendingReloads.shift());
  }
}


function _reload(path) {
  if (_reloadingDisabled) {
    if ($.inArray(path, _pendingReloads) < 0) {
      _pendingReloads.push(path);
    }
    return;
  }

  _disableReloads();
  $.ajax({
    url: 'list',
    type: 'GET',
    data: {path: path},
    dataType: 'json'
  }).fail(function(jqXHR, textStatus, errorThrown) {
    _showError("无法获取列表内容 - \"" + path + "\"", textStatus, errorThrown);
  }).done(function(data, textStatus, jqXHR) {
    var scrollPosition = $(document).scrollTop();
    data.sort(function(a,b) {
      return b.ctime - a.ctime;
    });
    if (path != _path) {
      $("#path").empty();
      if (_isRoot(path)) {
        $("#path").append('<li class="active">' + _device + '</li>');
      } else {
        $("#path").append('<li data-path="/"><a style="cursor:pointer">' + _device + '</a></li>');
        var components = path.slice(_root.length-1).split("/").slice(1, -1);
        for (var i = 0; i < components.length - 1; ++i) {
          var subpath = "/" + components.slice(0, i + 1).join("/") + "/";
          $("#path").append('<li data-path="' + subpath + '"><a style="cursor:pointer">' + components[i] + '</a></li>');
        }
        $("#path > li").click(function(event) {
          _reload(_root.concat($(this).data("path").slice(1)));
          event.preventDefault();
        });
        $("#path").append('<li class="active">' + components[components.length - 1] + '</li>');
      }
      _path = path;
    }

    $("#listing").empty();
    for (var i = 0, file; file = data[i]; ++i) {
      var trObj = $(tmpl("template-listing", file)).data(file);
      trObj.appendTo("#listing");
      if (_isHome(data[i].path)) {
        trObj.find(".button-move").hide();
        trObj.find(".button-delete").hide();
      }
    }

    $(".edit").editable(function(value, settings) {
      var name = $(this).parent().parent().data("name");
      if (value != name) {
        var path = $(this).parent().parent().data("path");
        $.ajax({
          url: 'move',
          type: 'POST',
          data: {oldPath: path, newPath: _path + value},
          dataType: 'json'
        }).fail(function(jqXHR, textStatus, errorThrown) {
          _showError("无法移动\"" + path + "\"到\"" + _path + value + "\"", textStatus, errorThrown);
        }).always(function() {
          _reload(_path);
        });
      }
      return value;
    }, {
      onedit: function(settings, original) {
        _disableReloads();
      },
      onsubmit: function(settings, original) {
        _enableReloads();
      },
      onreset: function(settings, original) {
        var input = $(original).find('input');
        var tr = input.parent().parent().parent().parent();
        var value = $.trim(input.val());
        var name = tr.data("name");

        if(value == "") {
          _showError("无效的命名 - ''", "ERROR", "");
        }

        if (value != "" && name != "" && value != name) {
          $.ajax({
            url: 'move',
            type: 'POST',
            data: {oldPath: _path + name, newPath: _path + value},
            dataType: 'json'
          }).fail(function(jqXHR, textStatus, errorThrown) {
            _showError("无法移动\"" + _path + name + "\"到\"" + _path + value + "\"", textStatus, errorThrown);
          }).always(function() {
            _reload(_path);
          });
        }

        _enableReloads();
      },
      tooltip: 'Click to rename...'
    });

    $(".button-download").click(function(event) {
      var path = $(this).parent().parent().data("path");
      setTimeout(function() {
        window.location = "download?path=" + encodeURIComponent(path);
      }, 0);
    });

    $(".button-open").click(function(event) {
      var path = $(this).parent().parent().data("path");
      _reload(path);
    });

    $(".button-move").click(function(event) {
      var path = $(this).parent().parent().data("path");
      if (path[path.length - 1] == "/") {
        path = path.slice(0, path.length - 1);
      }
      $("#move-input").data("path", path);
      $("#move-input").val(path);
      $("#move-modal").modal("show");
    });

    $(".button-delete").click(function(event) {
      var path = $(this).parent().parent().data("path");

      if (_isHome(path) || _isRoot(path)) {
        _showError("没有权限删除\"" + path + "\"", "ERROR", "");
      } else {
        $.ajax({
          url: 'delete',
          type: 'POST',
          data: {path: path},
          dataType: 'json'
        }).fail(function(jqXHR, textStatus, errorThrown) {
          _showError("无法删除\"" + path + "\"", textStatus, errorThrown);
        }).always(function() {
          _reload(_path);
        });
      }
    });

    $(document).scrollTop(scrollPosition);
  }).always(function() {
    _enableReloads();
  });
}

$(document).ready(function() {

  // Workaround Firefox and IE not showing file selection dialog when clicking on "upload-file" <button>
  // Making it a <div> instead also works but then it the button doesn't work anymore with tab selection or accessibility
  $("#upload-file").click(function(event) {
    $("#fileupload").click();
  });

  // Prevent event bubbling when using workaround above
  $("#fileupload").click(function(event) {
    event.stopPropagation();
    if(_isRoot(_path)){
      _showError("没有权限上传内容到\"" + _path + "\"", "ERROR", "");
      return false;
    }
  });

  $("#upload-folder").click(function(event) {
    $("#fileupload-folder").click();
  });

  $("#fileupload-folder").click(function(event) {
    event.stopPropagation();
    if(_isRoot(_path)){
      _showError("没有权限上传内容到\"" + _path + "\"", "ERROR", "");
      return false;
    }
  });

  $("#fileupload").fileupload({
    dropZone: $(document),
    pasteZone: null,
    autoUpload: true,
    sequentialUploads: true,
    // limitConcurrentUploads: 2,
    // forceIframeTransport: true,
    // maxChunkSize: MAX_CHUNK_SIZE,

    url: 'upload',
    type: 'POST',
    dataType: 'json',

    start: function(e) {
      $(".uploading").show();
    },

    stop: function(e) {
      $(".uploading").hide();
    },

    add: function(e, data) {
      var file = data.files[0];
      if(_isValidFileType(file.name)) {
        data.formData = {
          path: _path
        };
        data.context = $(tmpl("template-uploads", {
          path: _path + file.name
        })).appendTo("#uploads");
        var jqXHR = data.submit();
        data.context.find("button").click(function(event) {
          jqXHR.abort();
        });
      }
      else {
        _showErrors("不支持上传此类型文件 - \"" + file.name + "\"", "WARNING", "");
      }
    },

    progress: function(e, data) {
      var progress = parseInt(data.loaded / data.total * 100, 10);
      data.context.find(".progress-bar").css("width", progress + "%");
    },

    done: function(e, data) {
      _reload(_path);
    },

    fail: function(e, data) {
      var file = data.files[0];
      if (data.errorThrown != "abort") {
        _showError("上传文件失败 \"" + file.name + "\" to \"" + _path + "\"", data.textStatus, data.errorThrown);
      }
    },

    always: function(e, data) {
      data.context.remove();
    },

  });
  // Init folder upload
$("#fileupload-folder").fileupload({
    dropZone: $(document),
    pasteZone: null,
    autoUpload: true,
    sequentialUploads: true,
    url: 'upload',
    type: 'POST',
    dataType: 'json',

    start: function(e) {
      $(".uploading").show();
    },

    stop: function(e) {
      $(".uploading").hide();
    },

    add: function (e, data) {
      var file = data.files[0];
      var relativePath = file.webkitRelativePath || "";
      
      // 1. 校验文件类型
      if (!_isValidFileType(file.name)) {
        _showErrors('不支持的文件类型: ' + file.name, "WARNING", "");
        return;
      }
  
      // 2. 创建父目录（同步阻塞）
      createParentFolders(_path, relativePath)
        .then(function() {
          // 3. 设置上传路径
          var serverDir = _normalizePath(_path) + 
            relativePath.split('/').slice(0, -1).join('/') + '/';
            
          data.formData = {
            path: serverDir,
            relativePath: relativePath
          };
  
          // 4. 提交上传
          data.context = $(tmpl("template-uploads", {
            path: serverDir + file.name
          })).appendTo("#uploads");
          
          return data.submit();
        })
        .catch(function(error) {
          _showError(error, "ERROR", "");
          if (data.context) data.context.remove();
        });
    },

    progress: function(e, data) {
      var progress = parseInt(data.loaded / data.total * 100, 10);
      data.context.find(".progress-bar").css("width", progress + "%");
    },

    done: function(e, data) {
      _reload(_path);
    },

    fail: function(e, data) {
      var file = data.files[0];
      if (data.errorThrown != "abort") {
        _showError("上传文件失败 \"" + file.name + "\" to \"" + _path + "\"", data.textStatus, data.errorThrown);
      }
    },

    always: function(e, data) {
      data.context.remove();
    }
});

  $("#create-input").keypress(function(event) {
    if (event.keyCode == ENTER_KEYCODE) {
      $("#create-confirm").click();
    };
  });

  $("#create-modal").on("shown.bs.modal", function(event) {
    $("#create-input").focus();
    $("#create-input").select();
  });

  $("#create-folder").click(function(event) {
    if (_isRoot(_path)) {
      _showError("没有权限添加内容到\"" + _path + "\"", "ERROR", "");
    } else {
      $("#create-input").val("新建文件夹");
      $("#create-modal").modal("show");
    }
  });

  $("#create-confirm").click(function(event) {
    $("#create-modal").modal("hide");
    var name = $("#create-input").val();
    if ((name != "") && (!_isRoot(path+name))) {
      $.ajax({
        url: 'create',
        type: 'POST',
        data: {path: _path + name},
        dataType: 'json'
      }).fail(function(jqXHR, textStatus, errorThrown) {
        _showError("无法创建\"" + name + "\"到\"" + _path + "\"", textStatus, errorThrown);
      }).always(function() {
        _reload(_path);
      });
    }
  });

  $("#move-input").keypress(function(event) {
    if (event.keyCode == ENTER_KEYCODE) {
      $("#move-confirm").click();
    };
  });

  $("#move-modal").on("shown.bs.modal", function(event) {
    $("#move-input").focus();
    $("#move-input").select();
  })

  $("#move-confirm").click(function(event) {
    $("#move-modal").modal("hide");
    var oldPath = $("#move-input").data("path");
    var newPath = $("#move-input").val();

    // eg: invalid: "/", "/../", "$(_root)"
    if ((newPath.length <= _root.length) || (!_isContainHome(newPath))
      || _isRoot(newPath) || _isRoot(oldPath)) {
      _showError("没有权限移动内容到此目录 - \"" + newPath + "\"", "ERROR", "");
    } else if (newPath != oldPath) {
      $.ajax({
        url: 'move',
        type: 'POST',
        data: {oldPath: oldPath, newPath: newPath},
        dataType: 'json'
      }).fail(function(jqXHR, textStatus, errorThrown) {
        _showError("无法移动\"" + oldPath + "\"到\"" + newPath + "\"", textStatus, errorThrown);
      }).always(function() {
        _reload(_path);
      });
    }
  });

  $("#reload").click(function(event) {
    _reload(_path);
  });

  _reload(_home);

});
