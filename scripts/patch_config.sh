#!/bin/bash

custom_sed() {
  local search_pattern="$1"
  local replacement="$2"
  local file="$3"

  if grep -qE "$search_pattern" "$file"; then
    sed -i "s/${search_pattern}/${replacement}/" "$file" && \
    echo "Modified: [$search_pattern] -> [$replacement] in $file"
  else
    echo "NOT FOUND: [$search_pattern] in $file"
  fi
}

custom_sed '^# CONFIG_WGET is not set' 'CONFIG_WGET=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_LONG_OPTIONS is not set' 'CONFIG_FEATURE_WGET_LONG_OPTIONS=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_STATUSBAR is not set' 'CONFIG_FEATURE_WGET_STATUSBAR=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_FTP is not set' 'CONFIG_FEATURE_WGET_FTP=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_AUTHENTICATION is not set' 'CONFIG_FEATURE_WGET_AUTHENTICATION=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_TIMEOUT is not set' 'CONFIG_FEATURE_WGET_TIMEOUT=y' .config
custom_sed '^# CONFIG_FEATURE_WGET_HTTPS is not set' 'CONFIG_FEATURE_WGET_HTTPS=y' .config