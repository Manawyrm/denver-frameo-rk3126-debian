# Shared helpers for the build scripts, sourced after TOP is set.
#
# Environment (all optional):
#   BUILD_DIR      work directory            (default: <project>/build)
#   OUTPUT_DIR     where the results go      (default: <project>/output)
#   DOWNLOAD_DIR   download cache            (default: <project>/downloads)
#   JOBS           parallel make jobs        (default: nproc)
#   CROSS_COMPILE  toolchain prefix          (default: arm-linux-gnueabihf-)
#   SOURCE_DATE_EPOCH  build timestamp for reproducible builds
#                  (default: last commit of the project, else now)

BUILD_DIR=${BUILD_DIR:-$TOP/build}
OUTPUT_DIR=${OUTPUT_DIR:-$TOP/output}
DOWNLOAD_DIR=${DOWNLOAD_DIR:-$TOP/downloads}
JOBS=${JOBS:-$(nproc)}
CROSS_COMPILE=${CROSS_COMPILE:-arm-linux-gnueabihf-}

export ARCH=arm
export CROSS_COMPILE

# Reproducible builds: compilers take __DATE__/__TIME__ from it, kbuild and
# U-Boot their version timestamps, packages their file timestamps
if [ -z "${SOURCE_DATE_EPOCH:-}" ]
then
	SOURCE_DATE_EPOCH=$(git -C "$TOP" log -1 --format=%ct 2> /dev/null || date +%s)
fi
export SOURCE_DATE_EPOCH
umask 022

log()
{
	echo
	echo "=== $* ==="
}

die()
{
	echo "ERROR: $*" >&2
	exit 1
}

# check_tools TOOL...: fail if one of the tools is missing
check_tools()
{
	local missing=""
	local tool

	for tool in "$@"
	do
		if ! command -v "$tool" > /dev/null
		then
			missing="$missing $tool"
		fi
	done

	if [ -n "$missing" ]
	then
		die "missing tools:$missing (see README.md for the Debian packages)"
	fi
}

# fetch_file URL FILE SHA256: download URL to FILE (unless cached) and
# check its SHA-256
fetch_file()
{
	local url=$1
	local file=$2
	local sha256=$3

	mkdir -p "$(dirname "$file")"
	if [ ! -e "$file" ]
	then
		echo "Downloading $url"
		curl -fsSL --retry 3 -o "$file.part" "$url"
		mv "$file.part" "$file"
	fi

	if ! echo "$sha256  $file" | sha256sum -c --quiet
	then
		rm -f "$file"
		die "checksum mismatch for $file (deleted, run again)"
	fi
}

# fetch_git URL TAG COMMIT DIR: shallow clone of TAG into DIR (unless
# cached), which has to be COMMIT
fetch_git()
{
	local url=$1
	local tag=$2
	local commit=$3
	local dir=$4
	local head

	if [ ! -d "$dir" ]
	then
		echo "Cloning $url $tag"
		git -c advice.detachedHead=false clone -q --depth 1 -b "$tag" "$url" "$dir.part"
		mv "$dir.part" "$dir"
	fi

	head=$(git -C "$dir" rev-parse HEAD)
	if [ "$head" != "$commit" ]
	then
		die "$dir is at $head, expected $commit for $tag"
	fi
}

# apply_patches DIR SRC: apply DIR/*.patch to the tree SRC, in order
apply_patches()
{
	local p

	for p in "$1"/*.patch
	do
		echo "Applying $(basename "$p")"
		patch -d "$2" -p1 --batch --forward --no-backup-if-mismatch -s < "$p"
	done
}

# check_config FRAGMENT CONFIG: every option of the fragment must have its
# value in the final .config (Kconfig silently drops options whose
# dependencies are not met)
check_config()
{
	local fragment=$1
	local config=$2
	local errors=0
	local line
	local name
	local value
	local actual

	while read -r line
	do
		case "$line" in
			CONFIG_*=*)
				name=${line%%=*}
				value=${line#*=}
				;;
			"# CONFIG_"*" is not set")
				name=${line#\# }
				name=${name%% *}
				value=n
				;;
			*)
				continue
				;;
		esac

		actual=$(grep -E "^$name=|^# $name is not set" "$config" || true)
		if [ "$value" = "n" ]
		then
			if [ -n "$actual" ] && [ "$actual" != "# $name is not set" ]
			then
				echo "  $name: wanted n, got ${actual#*=}"
				errors=$((errors + 1))
			fi
		elif [ "$actual" != "$name=$value" ]
		then
			echo "  $name: wanted $value, got ${actual:-nothing}"
			errors=$((errors + 1))
		fi
	done < "$fragment"

	if [ $errors -gt 0 ]
	then
		die "$errors option(s) of $(basename "$fragment") not applied"
	fi
	echo "All options of $(basename "$fragment") applied."
}

# run_x86 COMMAND...: run an x86-64 binary, through qemu-user on other hosts
run_x86()
{
	if [ "$(uname -m)" = "x86_64" ]
	then
		"$@"
	else
		qemu-x86_64 -L /usr/x86_64-linux-gnu "$@"
	fi
}

# build_deb STAGE: build STAGE (with STAGE/DEBIAN/control) into OUTPUT_DIR
# as <Package>_<Version>_<Architecture>.deb, with reproducible timestamps
build_deb()
{
	local stage=$1
	local name

	name=$(sed -n 's/^Package: //p' "$stage/DEBIAN/control")
	name=${name}_$(sed -n 's/^Version: //p' "$stage/DEBIAN/control")
	name=${name}_$(sed -n 's/^Architecture: //p' "$stage/DEBIAN/control").deb

	find "$stage" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
	mkdir -p "$OUTPUT_DIR"
	dpkg-deb --root-owner-group -Zxz --build "$stage" "$OUTPUT_DIR/$name"
}
