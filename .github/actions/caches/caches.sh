#!/usr/bin/env bash
# Restores and saves the caches of a job in the bucket of the compiler cache, as Zstandard archives of tar named after their keys, which begin with the prefix of their layer.
# The command "restore <key> <save> <path>..." extracts the archive of the key when the bucket holds it. The value <save> is "missing" when the job saves the cache that the bucket lacks, which goes into the list that "save" uploads once the job succeeded, and "never" otherwise.
set -euo pipefail

export AWS_DEFAULT_REGION="$SCCACHE_REGION"
# The S3 API of R2 checks the checksums that a request asks for, and the AWS CLI asks for them only where the API requires them.
export AWS_REQUEST_CHECKSUM_CALCULATION=when_required
export AWS_RESPONSE_CHECKSUM_VALIDATION=when_required

list="$RUNNER_TEMP/varn-caches-to-save"

object() {
  echo "$1.tar.zst"
}

restore() {
  local key=$1 save=$2
  shift 2
  local answer
  if answer=$(aws s3api head-object --bucket "$SCCACHE_BUCKET" --key "$(object "$key")" --endpoint-url "$SCCACHE_ENDPOINT" 2>&1); then
    local start=$SECONDS
    aws s3 cp "s3://$SCCACHE_BUCKET/$(object "$key")" - --endpoint-url "$SCCACHE_ENDPOINT" --only-show-errors | zstd -d -q | tar -xf -
    echo "Restored the cache \"$key\" in $((SECONDS - start)) seconds."
    return
  fi

  # Only a missing object is a miss, and any other answer of the bucket, such as a refused token, stops the job.
  if [[ "$answer" != *"(404)"* ]]; then
    echo "::error::The bucket \"$SCCACHE_BUCKET\" did not answer for the cache \"$key\": $answer"
    exit 1
  fi
  echo "The bucket holds no cache \"$key\" yet."
  if [ "$save" = "missing" ]; then
    echo "$key $*" >> "$list"
  fi
}

save() {
  if [ ! -f "$list" ]; then
    echo "The job has no cache to save."
    return
  fi

  # Windows extracts no symbolic link whose target comes later in the archive, so its archives hold the files that the links name.
  local links=""
  if [ "$RUNNER_OS" = "Windows" ]; then
    links="--dereference"
  fi

  while read -r key paths; do
    local start=$SECONDS
    # The paths may be patterns, such as ".tools/haylen-shaderc-*", which the shell expands here.
    # shellcheck disable=SC2086
    tar $links -cf - $paths | zstd -q -T0 -3 | aws s3 cp - "s3://$SCCACHE_BUCKET/$(object "$key")" --endpoint-url "$SCCACHE_ENDPOINT" --only-show-errors
    echo "Saved the cache \"$key\" in $((SECONDS - start)) seconds."
  done < "$list"
}

"$@"
