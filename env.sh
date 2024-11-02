# This script should be sourced, not executed.

__realpath() {
    wdir="$PWD"; [ "$PWD" = "/" ] && wdir=""
    arg=$1
    case "$arg" in
        /*) scriptdir="${arg}";;
        *) scriptdir="$wdir/${arg#./}";;
    esac
    scriptdir="${scriptdir%/*}"
    echo "$scriptdir"
}

__script_dir(){
    if [ "$(uname -s)" = "Darwin" ]; then
        script_dir="$(__realpath "${self_path}")"
        script_dir="$(cd "${script_dir}" || exit 1; pwd)"
    else
        script_name="$(readlink -f "${self_path}")"
        script_dir="$(dirname "${script_name}")"
    fi
    if [ "$script_dir" = '.' ]
    then
       script_dir="$(pwd)"
    fi
    echo "$script_dir"
}

if [ -n "${BASH_SOURCE-}" ] && [ "${BASH_SOURCE[0]}" = "${0}" ]; then
  echo "This script should be sourced, not executed:"
  echo ". ${BASH_SOURCE[0]}"
  exit 1
fi

script_dir="$(__script_dir)"

echo "h9can path: ${script_dir}"

export H9CAN_PATH="${script_dir}"
export PATH="${script_dir}/tools:${PATH}"
