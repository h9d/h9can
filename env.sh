# This script should be sourced, not executed.

if [ -n "${BASH_SOURCE-}" ] && [ "${BASH_SOURCE[0]}" = "${0}" ]; then
  echo "This script should be sourced, not executed:"
  echo ". ${BASH_SOURCE[0]}"
  exit 1
fi

link=$(readlink -f "${BASH_SOURCE[${#BASH_SOURCE[@]} - 1]}")
script_dir="$(dirname ${link})"

echo "h9can path: ${script_dir}"

export H9CAN_PATH="${script_dir}"
export PATH="${script_dir}/tools:${PATH}"
