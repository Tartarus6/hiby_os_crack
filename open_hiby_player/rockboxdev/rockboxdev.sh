#!/bin/bash
set -e

# ----------------------------------------------------------------------
# Platform detection & tools
# ----------------------------------------------------------------------
system=$(uname -s)
if [ "$system" == "Darwin" ]; then
    parallel=$(sysctl -n hw.physicalcpu)
    READLINK=greadlink
    # TMP="$TMPDIR"
    SED=gsed
    PATH="$HOMEBREW_PREFIX/opt/gnu-sed/libexec/gnubin:${PATH}"
else
    READLINK=readlink
    parallel=$(nproc)
    # TMP=/tmp
    SED=sed
fi

INITIAL_DIR=$(pwd)
TMP="./tmp"
mkdir -p "$TMP"

# ----------------------------------------------------------------------
# Configuration (user overridable via environment or command line)
# ----------------------------------------------------------------------
HOST_PREFIX="${RBDEV_HOST_PREFIX:-$PWD/rockbox-toolchain}"   # where cross tools & sysroot go
dlwhere="${RBDEV_DOWNLOAD:-$TMP/rbdev-dl}"                 # download cache
builddir="${RBDEV_BUILD:-$TMP/rbdev-build}"                # build directory
MAKEFLAGS="${MAKEFLAGS:-}"                                 # extra make flags
RBDEV_RESTART="${RBDEV_RESTART:-}"                         # restart at given step

# GNU make
if [ -f "$(which gmake 2>/dev/null)" ]; then
    make="gmake"
else
    make="make"
fi
makever=$($make -v | $SED -n '1p' | $SED -e 's/.* \([0-9]*\)\.\([0-9]*\).*/\1\2/')
[ $parallel -gt 1 ] && make_parallel="-j$parallel"

# Script location
rockboxdevdir="$( $READLINK -f "$( dirname "${BASH_SOURCE[0]}" )" )"
patch_dir="$rockboxdevdir/toolchain-patches"

# Mirrors
: "${GNU_MIRROR:=https://mirrors.kernel.org/gnu}"
: "${LINUX_MIRROR:=https://www.kernel.org/pub/linux}"

# Required host tools
reqtools="gcc g++ xz bzip2 gzip $make patch makeinfo automake libtool autoconf"

# ----------------------------------------------------------------------
# Helper functions (mostly unchanged)
# ----------------------------------------------------------------------
findtool() {
    local file="$1"
    IFS=":"
    for path in $PATH; do
        if test -f "$path/$file"; then
            echo "$path/$file"
            return
        fi
    done
}

findlib() {
    local lib="$1"
    if ! gcc -l$lib 2>&1 | grep -q -- "-l$lib"; then
        echo "ok"
        return
    fi
}

checklib() {
    local missingtools=""
    for t in "$@"; do
        local lib=$(findlib $t)
        if test -z "$lib"; then
            echo "ROCKBOXDEV: library \"$t\" is required."
            missingtools="yes"
        fi
    done
    if [ -n "$missingtools" ]; then
        echo "ROCKBOXDEV: Please install missing libraries."
        exit 1
    fi
}

version_lt() {
    local ltver=$(printf "$1\n$2" | sort -V | head -n 1)
    [ "$1" = "$ltver" ] && true || false
}

getfile_ex() {
    local out_file="$dlwhere/$1"
    local srv_file="$2"
    if test -f $out_file; then
        echo "ROCKBOXDEV: Skipping download of $1 (already exists)"
        return
    fi
    local tool=$(findtool curl)
    if test -z "$tool"; then
        tool=$(findtool wget)
        if test -n "$tool"; then
            echo "ROCKBOXDEV: Downloading $1 using wget"
            tool="$tool -T 60 -O "
        else
            echo "ROCKBOXDEV: No downloader tool found (curl/wget)."
            exit
        fi
    else
        echo "ROCKBOXDEV: Downloading $1 using curl"
        tool="$tool -fLo "
    fi

    shift 2
    for url in "$@"; do
        echo "ROCKBOXDEV: try $url/$srv_file"
        if $tool "$out_file" "$url/$srv_file"; then
            return
        fi
    done
    echo "ROCKBOXDEV: couldn't download the file!"
    exit
}

getfile() {
    getfile_ex "$1" "$1" "$2"
}

gettool() {
    local toolname="$1"
    local version="$2"
    local ext="tar.bz2"
    local file="$toolname-$version"
    local srv_file="$toolname-$version"
    case $toolname in
        gcc)
            if version_lt "$version" "4.7"; then
                srv_file="gcc-core-$version"
            fi
            url="$GNU_MIRROR/gcc/gcc-$version"
            if ! version_lt "$version" "7.2"; then
                ext="tar.xz"
            fi
            ;;
        binutils)
            url="$GNU_MIRROR/binutils"
            if ! version_lt "$version" "2.28.1"; then
                ext="tar.xz"
            fi
            ;;
        glibc)
            url="$GNU_MIRROR/glibc"
            if ! version_lt "$version" "2.11"; then
                ext="tar.xz"
            fi
            ;;
        linux)
            case "$version" in
                2.6.*.*)
                    longterm_ver="${version%.*}"
                    top_dir="v2.6"
                    ;;
                3.*)
                    longterm_ver=""
                    top_dir="v3.x"
                    ;;
                *) echo "ROCKBOXDEV: I don't know how to handle kernel version $version"; exit ;;
            esac
            base_url="$LINUX_MIRROR/kernel/$top_dir"
            url="$base_url $base_url/longterm/v$longterm_ver $base_url/longterm"
            ext="tar.gz"
            ;;
        *) echo "ROCKBOXDEV: Bad toolname $toolname"; exit ;;
    esac
    getfile_ex "$file.$ext" "$srv_file.$ext" $url
}

extract() {
    if [ -d "$1" ]; then
        echo "ROCKBOXDEV: Skipping extraction of $1 (already done)"
        return
    fi
    echo "ROCKBOXDEV: extracting $1"
    if [ -f "$dlwhere/$1.tar.bz2" ]; then
        tar xjf "$dlwhere/$1.tar.bz2"
    elif [ -f "$dlwhere/$1.tar.gz" ]; then
        tar xzf "$dlwhere/$1.tar.gz"
    elif [ -f "$dlwhere/$1.tar.xz" ]; then
        tar xJf "$dlwhere/$1.tar.xz"
    else
        echo "ROCKBOXDEV: unknown compression for $1"
        exit
    fi
}

run_cmd() {
    local logfile="$1"
    shift
    echo "Running '$@'" >>$logfile
    if ! $@ >> "$logfile" 2>&1; then
        echo "ROCKBOXDEV: an error occurred, please see $logfile"
        exit 1
    fi
}

check_restart() {
    if [ "x$RBDEV_RESTART" = "x" ]; then
        return 0
    elif [ "$1" = "$RBDEV_RESTART" ]; then
        RBDEV_RESTART=""
        return 0
    else
        return 1
    fi
}

# buildtool <tool> <version> <configure_opts> <make_opts> <install_opts>
# Uses global $prefix (set before call) and $make, $make_parallel, $patch_dir
buildtool() {
    local tool="$1"
    local version="$2"
    local config_opt="$3"
    local make_opts="$4"
    local install_opts="$5"
    local toolname="$tool-$version"
    local logfile="$builddir/build-$toolname.log"
    local stepname="${RESTART_STEP:-$tool}"

    if ! check_restart "$stepname"; then
        echo "ROCKBOXDEV: Skipping step '$stepname' as requested"
        return
    fi
    echo "ROCKBOXDEV: Starting step '$stepname'"
    echo "ROCKBOXDEV: logging to $logfile"
    rm -f "$logfile"

    mkdir "build-$toolname"
    cd "build-$toolname"

    local cfg_dir
    case "$tool" in
        linux|alsa-lib)
            cp -r ../$toolname/* .
            cfg_dir="."
            ;;
        *)
            cfg_dir="../$toolname"
            ;;
    esac

    # Special CXXFLAGS for gcc stages
    local CXXFLAGS=""
    if [ "$RESTART_STEP" == "gcc-stage1" ] ; then
        CXXFLAGS="-std=gnu++03"
    elif [ "$RESTART_STEP" == "gcc-stage2" ] ; then
        CXXFLAGS="-std=gnu++11"
    fi

    local cflags='-U_FORTIFY_SOURCE -fgnu89-inline -O2'
    if [ "$tool" == "glibc" ]; then
        cflags="$cflags -fcommon"
    elif [ "$tool" == "glib" ]; then
        if version_lt "$version" "2.47.5"; then
            run_cmd "$logfile" $SED -i -e 's/m4_copy/m4_copy_force/g' "$cfg_dir/m4macros/glib-gettext.m4"
            run_cmd "$logfile" $SED -i 's/tests//' "$cfg_dir/gio/Makefile.am"
            run_cmd "$logfile" autoreconf -fiv "$cfg_dir"
            config_opt="$config_opt --disable-gtk-doc"
        fi
        cflags="$cflags -Wno-format-nonliteral -Wno-format-overflow"
    fi

    # Some tools (like zlib) don't support --disable-docs/--disable-tests
    local extra_flags=""
    if [ "$tool" != "zlib" ]; then
        extra_flags="--disable-docs --disable-tests"
    fi

    if [ "$config_opt" != "NO_CONFIGURE" ]; then
        echo "ROCKBOXDEV: $toolname/configure"
        CFLAGS="$cflags" CXXFLAGS="$CXXFLAGS" run_cmd "$logfile" \
            "$cfg_dir/configure" "--prefix=$prefix" \
            $extra_flags $config_opt
    fi

    if [ "$make_opts" != "NO_MAKE" ]; then
        echo "ROCKBOXDEV: $toolname/make"
        run_cmd "$logfile" $make $make_parallel $make_opts
    fi

    if [ -z "$install_opts" ]; then
        install_opts="install"
    fi
    echo "ROCKBOXDEV: $toolname/make install"
    run_cmd "$logfile" $make $install_opts

    cd ..
    rm -rf "build-$toolname"
    if [ "$stepname" != "gcc-stage1" ] ; then
        rm -rf "$toolname"
    fi
}

# ----------------------------------------------------------------------
# Main Hiby toolchain builder
# ----------------------------------------------------------------------
build_hiby_toolchain() {
    local target="mipsel-rockbox-linux-gnu"
    local sysroot="$HOST_PREFIX/$target/sysroot"
    local arch="mipsel"   # for kernel headers

    # Ensure system libraries for gcc (gmp, mpfr, mpc)
    checklib "mpc" "gmp" "mpfr"

    mkdir -p "$builddir" "$dlwhere"

    # Versions
    local binutils_ver="2.38"
    local gcc_ver="9.5.0"
    local linux_ver="3.10.108"
    local glibc_ver="2.27"
    local alsalib_ver="1.0.26"
    local libffi_ver="3.2.1"
    local zlib_ver="1.2.13"
    local glib_ver="2.46.2"
    local expat_ver="2.1.0"
    local dbus_ver="1.10.2"

    local binutils_patches="binutils-c23.patch"
    local glibc_patches="glibc-227-make44.patch"
    local linux_patches="linux-c23.patch"
    local glibc_opts="--enable-kernel=3.2 --enable-oldest-abi=2.16 --disable-werror"

    cd "$builddir"

    # Download all sources
    gettool "binutils" "$binutils_ver"
    gettool "gcc" "$gcc_ver"
    gettool "linux" "$linux_ver"
    gettool "glibc" "$glibc_ver"

    extract "binutils-$binutils_ver"
    extract "gcc-$gcc_ver"
    extract "linux-$linux_ver"
    extract "glibc-$glibc_ver"

    # Apply patches
    for p in $binutils_patches; do
        echo "ROCKBOXDEV: applying binutils patch $p"
        (cd "$builddir/binutils-$binutils_ver" && patch -p1 < "$patch_dir/$p") || exit
    done
    for p in $glibc_patches; do
        echo "ROCKBOXDEV: applying glibc patch $p"
        (cd "$builddir/glibc-$glibc_ver" && patch -p1 < "$patch_dir/$p") || exit
    done
    for p in $linux_patches; do
        echo "ROCKBOXDEV: applying linux patch $p"
        (cd "$builddir/linux-$linux_ver" && patch -p1 < "$patch_dir/$p") || exit
    done

    # 1. Binutils
    prefix="$HOST_PREFIX"
    RESTART_STEP="" \
    buildtool "binutils" "$binutils_ver" \
        "--target=$target --disable-werror --with-sysroot=$sysroot --disable-nls" "" ""

    # 2. GCC stage1 (without headers, no threads)
    prefix="$HOST_PREFIX"
    RESTART_STEP="gcc-stage1" \
    buildtool "gcc" "$gcc_ver" \
        "--enable-languages=c --target=$target --without-headers --disable-threads \
         --disable-libgomp --disable-libmudflap --disable-libssp --disable-libquadmath \
         --disable-shared --with-newlib --disable-libitm --disable-libsanitizer --disable-libatomic" "" ""

    # 3. Linux kernel headers
    if [ "$arch" == "mipsel" ]; then arch="mips"; fi
    prefix="$HOST_PREFIX"
    local linux_opts="O=. ARCH=$arch INSTALL_HDR_PATH=$sysroot/usr/"
    RESTART_STEP="linux-headers" \
    buildtool "linux" "$linux_ver" "NO_CONFIGURE" \
        "$linux_opts headers_install" "$linux_opts headers_check"

    # 4. glibc (installs to sysroot)
    prefix="/usr"
    RESTART_STEP="glibc" \
    buildtool "glibc" "$glibc_ver" \
        "--target=$target --host=$target --build=$MACHTYPE \
         --with-__thread --with-headers=$sysroot/usr/include $glibc_opts" \
        "" "install install_root=$sysroot"

    # 5. GCC stage2 (full C/C++ with sysroot)
    prefix="$HOST_PREFIX"
    RESTART_STEP="gcc-stage2" \
    buildtool "gcc" "$gcc_ver" \
        "--enable-languages=c,c++ --target=$target --with-sysroot=$sysroot" "" ""

    # Add the cross compiler to PATH for subsequent library builds
    PATH="$HOST_PREFIX/bin:$PATH"

    # ------------------------------------------------------------------
    # Target libraries (Bluetooth support) – all installed into sysroot
    # ------------------------------------------------------------------
    # 6. alsa-lib
    getfile_ex "alsa-lib-$alsalib_ver.tar.bz2" "alsa-lib-$alsalib_ver.tar.bz2" \
        "https://www.alsa-project.org/files/pub/lib"
    extract "alsa-lib-$alsalib_ver"
    prefix="/usr" \
    buildtool "alsa-lib" "$alsalib_ver" \
        "--host=$target --disable-python" "" "install DESTDIR=$sysroot"

    # 7. libffi
    getfile_ex "libffi-$libffi_ver.tar.gz" "libffi-$libffi_ver.tar.gz" \
        "https://sourceware.org/pub/libffi"
    extract "libffi-$libffi_ver"
    prefix="/usr" \
    buildtool "libffi" "$libffi_ver" \
        "--includedir=/usr/include --host=$target" "" "install DESTDIR=$sysroot"
    (cd $sysroot/usr/include ; ln -sf ../lib/libffi-$libffi_ver/include/ffi.h . ; ln -sf ../lib/libffi-$libffi_ver/include/ffitarget.h .)

    # 8. zlib (skip --disable-docs because it's unsupported)
    getfile_ex "zlib-$zlib_ver.tar.gz" "zlib-$zlib_ver.tar.gz" \
        "https://www.zlib.net/fossils"
    extract "zlib-$zlib_ver"
    CHOST=$target prefix="/usr" \
    buildtool "zlib" "$zlib_ver" "" "" "install DESTDIR=$sysroot"

    # 9. glib
    getfile_ex "glib-$glib_ver.tar.xz" "glib-$glib_ver.tar.xz" \
        "https://download.gnome.org/sources/glib/2.46"
    extract "glib-$glib_ver"
    prefix="/usr" \
    buildtool "glib" "$glib_ver" \
        "--host=$target --with-sysroot=$sysroot --disable-libelf \
         glib_cv_stack_grows=no glib_cv_uscore=no \
         ac_cv_func_posix_getpwuid_r=yes ac_cv_func_posix_getgrgid_r=yes \
         CFLAGS=-Wno-error=format-nonliteral" "" "install DESTDIR=$sysroot"

    # 10. expat
    getfile_ex "expat-$expat_ver.tar.gz" "expat-$expat_ver.tar.gz" \
        "https://src.fedoraproject.org/repo/pkgs/expat/expat-2.1.0.tar.gz/dd7dab7a5fea97d2a6a43f511449b7cd"
    extract "expat-$expat_ver"
    prefix="/usr" \
    buildtool "expat" "$expat_ver" \
        "--host=$target --includedir=/usr/include --enable-abstract-sockets" "" "install DESTDIR=$sysroot"

    # 11. dbus
    getfile_ex "dbus-$dbus_ver.tar.gz" "dbus-$dbus_ver.tar.gz" \
        "https://dbus.freedesktop.org/releases/dbus"
    extract "dbus-$dbus_ver"
    prefix="/usr" \
    buildtool "dbus" "$dbus_ver" \
        "--host=$target --with-sysroot=$sysroot --includedir=/usr/include \
         --enable-abstract-sockets ac_cv_lib_expat_XML_ParserCreate_MM=yes \
         --disable-systemd --disable-launchd --enable-x11-autolaunch=no \
         --with-x=no --disable-selinux --disable-apparmor --disable-doxygen-docs" "" "install DESTDIR=$sysroot"

    echo "ROCKBOXDEV: Hiby OS toolchain build complete."
}

# ----------------------------------------------------------------------
# Parse command line
# ----------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --help)
            echo "Usage: $0 [--prefix=PREFIX] [--dlwhere=DIR] [--builddir=DIR] [--makeflags=FLAGS] [--restart=STEP]"
            echo "  PREFIX defaults to ./rockbox-toolchain (host install + sysroot)"
            exit 0
            ;;
        --prefix=*)
            HOST_PREFIX="${1#*=}"
            shift
            ;;
        --dlwhere=*)
            dlwhere="${1#*=}"
            shift
            ;;
        --builddir=*)
            builddir="${1#*=}"
            shift
            ;;
        --makeflags=*)
            export MAKEFLAGS="${1#*=}"
            shift
            ;;
        --restart=*)
            RBDEV_RESTART="${1#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ----------------------------------------------------------------------
# Pre‑flight checks
# ----------------------------------------------------------------------
# Required tools
missing=""
for t in $reqtools; do
    if test -z "$(findtool $t)"; then
        echo "ROCKBOXDEV: \"$t\" is required."
        missing="yes"
    fi
done
if [ -n "$missing" ]; then
    echo "ROCKBOXDEV: Please install missing tools."
    exit 1
fi

# GNU Make
if ! $make -v | grep -q GNU ; then
    echo "ROCKBOXDEV: GNU Make required."
    exit 1
fi

# Resolve paths (make absolute)
dlwhere=$($READLINK -f "$dlwhere" 2>/dev/null || echo "$dlwhere")
HOST_PREFIX=$($READLINK -f "$HOST_PREFIX" 2>/dev/null || echo "$HOST_PREFIX")
builddir=$($READLINK -f "$builddir" 2>/dev/null || echo "$builddir")

mkdir -p "$dlwhere" "$HOST_PREFIX" "$builddir"

echo "Host install prefix: $HOST_PREFIX"
echo "Download directory:  $dlwhere"
echo "Build directory:     $builddir"
echo "Restart step:        $RBDEV_RESTART"
echo "MAKEFLAGS:           $MAKEFLAGS"

# ----------------------------------------------------------------------
# Go!
# ----------------------------------------------------------------------
PATH="$HOST_PREFIX/bin:$PATH"
build_hiby_toolchain

cd "$INITIAL_DIR"
rm -r "$TMP"

echo ""
echo "ROCKBOXDEV: Done!"
echo "Make sure your PATH includes $HOST_PREFIX/bin"
