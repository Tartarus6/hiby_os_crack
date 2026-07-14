#!/bin/sh


# 获得字符串中的第几个word
get_word()
{
    local str="$1"
    local n=$2
    local i=0

    for word in $str
    do
        if [ $i = $n ]; then
            echo $word
            return 0
        fi
        let i=i+1
    done

    return 1
}

# 获取关键字的值
get_key_word()
{
    local str="$1"
    local key="$2"
    local word=
    local tmp
    local len

    #　情况1　"key = words"
    tmp=`get_word "$str" 0`
    if [ "$tmp" = "$key" ]; then
        tmp=`get_word "$str" 1`
        if [ "$tmp" != "=" ]; then
            return 1
        fi

        echo ${str##*=}
        return 0
    fi

    #　情况2　"key=words"
    len=${#key}
    let len=len+1
    tmp=${str:0:$len}
    if [ "$tmp" = "$key=" ]; then
        echo ${str:$len}
        return 0
    fi

    return 1;
}

# 从文件中获得关键字
get_key_word_from_file()
{
    local file=$1
    local key=$2
    local is_find
    local result
    local result_save

    if [ ! -e "$file" ]; then
        echo "no such file: $file" 1>&2
        return 1
    fi

    while read line;
    do
        result=`get_key_word "$line" "$key"`
        if [ $? = 0 ]; then
            is_find=1
            result_save=$result
            continue
        fi
    done < "$file"

    if [ "$is_find" != "" ]; then
        echo $result_save
        return 0
    fi

    return 1
}

# 获得文件大小
size_file()
{
    local file=$1
    local result

    result="`ls -l -L $file`"
    if [ "$?" != "0" ]; then
        echo 0
        return 1
    fi

    result=`get_word "$result" 4`
    if [ "result" = "" ]; then
        echo 0
        return 1
    fi
    echo $result
}

# 等待文件出现
wait_file()
{
    local file=$1
    local count=$2
    local i=0

    while true;
    do
        if [ -e $file ]; then
            return 0
        fi

        if [ "$i" = "$count" ]; then
            return 1
        fi

        let i=i+1
        sleep 0.01
    done
}

# 获得文件的md5sum
md5sum_file()
{
    local file=$1
    local result

    result="`md5sum $file`"
    if [ $? != 0 ]; then
        echo "md5sum failed: $file" 1>&2
        exit 1
    fi

    result=`get_word "$result" 0`
    if [ "$result" = "" ]; then
        echo "md5sum get failed: $file" 1>&2
        exit 1
    fi
    echo $result
}

# wget 带重试次数
wget_retry()
{
    local site="$1"
    local file="$2"
    local count=$3
    local i=0

    while true;
    do
        echo "try to get $site" 1>&2
        wget "$site" -O "$file" -o /tmp/ota_wget_msg/$file.log
        if [ $? = 0 ]; then
            return 0;
        fi

        if [ $i = $count ]; then
            return 1
        fi

        let i=i+1
        sleep 1
    done
}

# 由mtd分区名获得mtd序号
mtd_name_to_num()
{
    local partition_name=$1
    local mtd_info=`cat /proc/mtd | grep -w \""$partition_name"\"`
    if [ "$mtd_info" = "" ]; then
        echo "mtd partition not find: $partition_name" 1>&2
        return 1
    fi

    local mtd_n=${mtd_info%%:*}
    if [ "$mtd_info" = "$mtd_n" ]; then
        echo "mtd_info not ok: $mtd_info" 1>&2
        return 1
    fi

    local n=${mtd_n#mtd}
    if [ "$n" = "$mtd_n" ]; then
        echo "mtd_info not ok2: $mtd_n" 1>&2
        return 1
    fi

    echo $n
}

# 由mtd分区名获得mtd设备节点
mtd_name_to_dev()
{
    local partition_name=$1
    local num

    num=`mtd_name_to_num $partition_name`
    if [ $? != 0 ]; then
        return 1
    fi

    local dev=/dev/mtd$num
    if [ ! -e $dev ]; then
        echo "why $dev not exist" 1>&2
        return 1
    fi

    echo $dev
}

# 写入字符串到mtd分区
mtd_write_str()
{
    local partition_name=$1
    local str=$2
    local dev

    dev=`mtd_name_to_dev "$partition_name"`
    if [ $? != 0 ]; then
        echo "failed to get mtd dev $partition_name" 1>&2
        return 1
    fi

    flash_erase $dev 0 1
    if [ $? != 0 ]; then
        echo "failed to erase $dev" 1>&2
        return 1
    fi

    printf "%-256s" "$str" | nandwrite -s 0 -p $dev -
    if [ $? != 0 ]; then
        echo "failed to write $dev" 1>&2
        return 1
    fi

    return 0
}

# 从mtd分区读取字符串
mtd_read_str()
{
    local partition_name=$1
    local dev

    dev=`mtd_name_to_dev "$partition_name"`
    if [ $? != 0 ]; then
        return 1
    fi

    nanddump -s 0 -l 256 $dev -a
    if [ $? != 0 ]; then
        return 1
    fi

    return 0
}

# 检查mtd 分区大小
mtd_check_size()
{
    local dev_path=$1
    local img_size=$2
    local percent=$3
    local mtd_info

    mtd_info=`mtdinfo $dev_path`
    if [ $? != 0 ]; then
        echo "mtd get info failed: $dev_path" 1>&2
        return 1
    fi

    mtd_info=`mtdinfo $dev_path | grep "Amount of eraseblocks"`
    if [ $? != 0 ]; then
        echo "mtd grep info failed: $dev_path" 1>&2
        return 1
    fi

    if [ "$mtd_info" = "" ]; then
        echo "why mtdinfo is empty: $dev_path" 1>&2
        return 1
    fi

    local tmp=${mtd_info##*(}
    if [ "$tmp" = "$mtd_info" ]; then
        echo "mtdinfo format is change: $dev_path: $mtd_info" 1>&2
        return 1
    fi

    local mtd_size=${tmp%% bytes*}
    if [ "$mtd_size" = "" ] || [ "$mtd_size" = "$tmp" ]; then
        echo "mtdinfo format is change: $dev_path: $mtd_info" 1>&2
        return 1
    fi

    if [ $mtd_size -lt $img_size ]; then
        echo "mtd size less than require size: $mtd_size $img_size" 1>&2
        return 1
    fi

    if [ "$percent" = "" ]; then
        return 0
    fi

    local mtd_size2
    let mtd_size2=mtd_size/1000*percent/100
    local img_size2
    let img_size2=img_size/1000
    if [ $mtd_size2 -lt $img_size2 ]; then
        echo "mtd size2 less than require size: $mtd_size $img_size $mtd_size2 $percent" 1>&2
        return 1
    fi

    return 0
}


