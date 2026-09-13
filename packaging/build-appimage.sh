#!/usr/bin/env bash
set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd -- "$script_dir/.." && pwd -P)"

# shellcheck source=packaging/appimage-tools.env
source "$script_dir/appimage-tools.env"

build_root="${INDE_APPIMAGE_BUILD_DIR:-$repo_root/build-appimage}"
output_dir="${INDE_APPIMAGE_OUTPUT_DIR:-$repo_root/dist}"
tools_dir="${INDE_APPIMAGE_TOOLS_DIR:-$build_root/tools}"
cmake_build_dir="$build_root/cmake"
appdir="$build_root/INDE.AppDir"
enchant_source_dir="$build_root/enchant-source"
enchant_build_dir="$build_root/enchant-build"
enchant_stage="$build_root/enchant-stage"
jobs="${INDE_APPIMAGE_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"
version="${INDE_APPIMAGE_VERSION:-$(sed -n 's/^project(inde VERSION \([^ ]*\).*/\1/p' "$repo_root/CMakeLists.txt")}"

die() {
    printf 'erro: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "comando obrigatório ausente: $1"
}

download_verified() {
    local url="$1"
    local expected="$2"
    local destination="$3"
    local temporary="${destination}.part"

    if [[ -f "$destination" ]] &&
       printf '%s  %s\n' "$expected" "$destination" | sha256sum --check --status; then
        return
    fi

    rm -f -- "$temporary"
    curl --fail --location --retry 3 "$url" --output "$temporary"
    printf '%s  %s\n' "$expected" "$temporary" | sha256sum --check --status || {
        rm -f -- "$temporary"
        die "checksum inválido para $url"
    }
    mv -- "$temporary" "$destination"
}

find_library() {
    local soname="$1"
    local path

    path="$(ldconfig -p 2>/dev/null | awk -v name="$soname" '$1 == name { print $NF; exit }')"
    [[ -n "$path" && -f "$path" ]] || die "biblioteca de runtime ausente: $soname"
    readlink -f -- "$path"
}

copy_package_notice() {
    local package="$1"
    local source="/usr/share/doc/$package/copyright"
    local destination="$appdir/usr/share/doc/inde/third-party/${package}-copyright"

    if [[ -f "$source" ]]; then
        install -Dm644 "$source" "$destination"
    else
        printf 'aviso: texto de licença do pacote %s não encontrado em %s\n' \
            "$package" "$source" >&2
    fi
}

[[ "$(uname -m)" == "x86_64" ]] || die "esta receita produz somente AppImage x86_64"
[[ -n "$version" ]] || die "não foi possível determinar a versão do projeto"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || die "INDE_APPIMAGE_JOBS deve ser um inteiro positivo"

for command_name in cmake curl desktop-file-validate appstreamcli sha256sum ldconfig awk install file grep make pkg-config tar; do
    require_command "$command_name"
done

if command -v magick >/dev/null 2>&1; then
    image_converter=(magick)
elif command -v convert >/dev/null 2>&1; then
    image_converter=(convert)
else
    die "ImageMagick é obrigatório para gerar o ícone AppImage de 256x256"
fi

mkdir -p -- "$build_root" "$tools_dir" "$output_dir"
linuxdeploy="$tools_dir/linuxdeploy-x86_64.AppImage"
gtk_plugin="$tools_dir/linuxdeploy-plugin-gtk.sh"
appimage_runtime="$tools_dir/runtime-x86_64"
enchant_archive="$tools_dir/enchant-${ENCHANT_VERSION}.tar.gz"
download_verified "$LINUXDEPLOY_URL" "$LINUXDEPLOY_SHA256" "$linuxdeploy"
download_verified "$LINUXDEPLOY_GTK_URL" "$LINUXDEPLOY_GTK_SHA256" "$gtk_plugin"
download_verified "$APPIMAGE_RUNTIME_URL" "$APPIMAGE_RUNTIME_SHA256" "$appimage_runtime"
download_verified "$ENCHANT_URL" "$ENCHANT_SHA256" "$enchant_archive"
chmod +x -- "$linuxdeploy" "$gtk_plugin"

desktop_file="$repo_root/packaging/io.github.inde.desktop"
metainfo_file="$repo_root/packaging/io.github.inde.appdata.xml"
appimage_icon="$build_root/inde-256.png"
desktop-file-validate "$desktop_file"
appstreamcli validate --no-net --pedantic "$metainfo_file"
if [[ "$version" != *-dev ]] &&
   ! grep -Fq "<release version=\"$version\"" "$metainfo_file"; then
    die "os metadados AppStream não registram a versão $version"
fi
"${image_converter[@]}" "$repo_root/assets/inde.png" -resize 256x256 \
    "$appimage_icon"

cmake -S "$repo_root" -B "$cmake_build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$cmake_build_dir" --parallel "$jobs"
ctest --test-dir "$cmake_build_dir" --output-on-failure

rm -rf -- "$appdir"
env DESTDIR="$appdir" cmake --install "$cmake_build_dir" --prefix /usr

# Distribution packages normally compile an absolute provider directory into
# Enchant. Build the pinned release in relocatable mode so APPDIR can redirect
# provider discovery without consulting /usr on the host.
rm -rf -- "$enchant_source_dir" "$enchant_build_dir" "$enchant_stage"
mkdir -p -- "$enchant_source_dir" "$enchant_build_dir"
tar -xzf "$enchant_archive" -C "$enchant_source_dir" --strip-components=1
(
    cd -- "$enchant_build_dir"
    "$enchant_source_dir/configure" \
        --prefix=/usr \
        --libdir=/usr/lib/x86_64-linux-gnu \
        --enable-relocatable \
        --with-hunspell \
        --without-nuspell \
        --without-aspell \
        --without-hspell \
        --without-voikko \
        --without-zemberek \
        --disable-static
    make --jobs "$jobs"
    make DESTDIR="$enchant_stage" install
)

enchant_library="$enchant_stage/usr/lib/x86_64-linux-gnu/libenchant-2.so.2"
hunspell_provider="$enchant_stage/usr/lib/x86_64-linux-gnu/enchant-2/enchant_hunspell.so"
hunspell_library="${INDE_HUNSPELL_LIBRARY:-$(find_library libhunspell-1.7.so.0)}"
dictionary_dir="${INDE_HUNSPELL_DICTIONARY_DIR:-/usr/share/hunspell}"

[[ -f "$enchant_library" ]] || die "libenchant não encontrada: $enchant_library"
[[ -f "$hunspell_provider" ]] || die "provider Enchant/Hunspell não encontrado: $hunspell_provider"
[[ -f "$hunspell_library" ]] || die "libhunspell não encontrada: $hunspell_library"
[[ -f "$dictionary_dir/pt_BR.aff" && -f "$dictionary_dir/pt_BR.dic" ]] ||
    die "dicionário Hunspell pt_BR ausente em $dictionary_dir"

# Seed the AppDir before dependency discovery. If these files are supplied only
# through --library, linuxdeploy may resolve the provider's SONAME back to the
# distro's non-relocatable libenchant and silently replace our build.
enchant_provider_dir="$appdir/usr/lib/x86_64-linux-gnu/enchant-2"
install -Dm755 "$enchant_library" "$appdir/usr/lib/libenchant-2.so.2"
install -Dm755 "$hunspell_library" "$appdir/usr/lib/libhunspell-1.7.so.0"
install -Dm755 "$hunspell_provider" "$enchant_provider_dir/enchant_hunspell.so"
ln -sfn ../../libenchant-2.so.2 "$enchant_provider_dir/libenchant-2.so.2"
ln -sfn ../../libhunspell-1.7.so.0 "$enchant_provider_dir/libhunspell-1.7.so.0"

export APPIMAGE_EXTRACT_AND_RUN=1
export DEPLOY_GTK_VERSION=4
export LINUXDEPLOY_OUTPUT_VERSION="$version"
export PATH="$tools_dir:$PATH"

"$linuxdeploy" \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/inde" \
    --desktop-file "$desktop_file" \
    --icon-file "$appimage_icon" \
    --plugin gtk

install -Dm644 "$dictionary_dir/pt_BR.aff" "$appdir/usr/share/hunspell/pt_BR.aff"
install -Dm644 "$dictionary_dir/pt_BR.dic" "$appdir/usr/share/hunspell/pt_BR.dic"
install -Dm644 /usr/share/enchant-2/enchant.ordering \
    "$appdir/usr/share/enchant-2/enchant.ordering"
install -Dm755 "$script_dir/appimage-runtime-hook.sh" \
    "$appdir/apprun-hooks/inde-spelling.sh"

# The upstream GTK hook defaults to X11 for compatibility. Preserve that
# default without overriding a user who explicitly requests another backend.
sed -i 's/^export GDK_BACKEND=x11 .*$/export GDK_BACKEND="${GDK_BACKEND:-x11}"/' \
    "$appdir/apprun-hooks/linuxdeploy-plugin-gtk.sh"
grep -Fq 'export GDK_BACKEND="${GDK_BACKEND:-x11}"' \
    "$appdir/apprun-hooks/linuxdeploy-plugin-gtk.sh" ||
    die "não foi possível tornar o backend GTK configurável"

copy_package_notice libenchant-2-2
copy_package_notice libhunspell-1.7-0
copy_package_notice hunspell-pt-br
install -Dm644 "$enchant_source_dir/COPYING.LIB" \
    "$appdir/usr/share/doc/inde/third-party/enchant-${ENCHANT_VERSION}-COPYING.LIB"

empty_enchant_prefix="$build_root/enchant-empty-prefix"
rm -rf -- "$empty_enchant_prefix"
mkdir -p -- "$empty_enchant_prefix/config"
isolated_spelling_probe="$(env \
    INDE_ENCHANT_PREFIX_DIR="$empty_enchant_prefix" \
    DICPATH="$empty_enchant_prefix/share/hunspell" \
    XDG_CONFIG_HOME="$empty_enchant_prefix/config" \
    LD_LIBRARY_PATH="$appdir/usr/lib" \
    "$cmake_build_dir/inde_proofreading_benchmark")"
if grep -q 'Hunspell via Enchant' <<<"$isolated_spelling_probe"; then
    printf '%s\n' "$isolated_spelling_probe" >&2
    die "a revisão ortográfica ainda depende do provider Enchant do host"
fi

spelling_probe="$(env \
    INDE_ENCHANT_PREFIX_DIR="$appdir/usr" \
    DICPATH="$appdir/usr/share/hunspell" \
    LD_LIBRARY_PATH="$appdir/usr/lib" \
    "$cmake_build_dir/inde_proofreading_benchmark")"
grep -q 'provider=Português (Brasil) — Hunspell via Enchant' <<<"$spelling_probe" || {
    printf '%s\n' "$spelling_probe" >&2
    die "o corretor ortográfico empacotado não ficou operacional"
}

artifact="$output_dir/INDE-${version}-x86_64.AppImage"
rm -f -- "$artifact" "$artifact.sha256"
export LDAI_OUTPUT="$artifact"
export LDAI_RUNTIME_FILE="$appimage_runtime"
# The recipe already ran a strict offline AppStream validation above. Avoid a
# second appimagetool pass that turns homepage reachability into a build input.
export LDAI_NO_APPSTREAM=1
"$linuxdeploy" --appdir "$appdir" --output appimage
(cd -- "$output_dir" && sha256sum "$(basename -- "$artifact")" > "$(basename -- "$artifact").sha256")
file "$artifact" | grep -q 'ELF.*executable' || die "artefato final não é um executável ELF"
"$artifact" --appimage-offset >/dev/null || die "runtime AppImage não reconheceu o artefato final"

printf 'AppImage: %s\nChecksum: %s\n' "$artifact" "$artifact.sha256"
