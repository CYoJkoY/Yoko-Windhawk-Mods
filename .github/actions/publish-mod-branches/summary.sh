# shellcheck shell=bash
#
# Helpers shared by the publish steps. Both steps write remote reasons into
# the job summary and into workflow commands, so the rules for one printable
# ASCII line, and for an escaped command message, belong in one place.

# A remote reason goes into a Markdown table cell, so it must stay one line of
# printable ASCII. A backtick would open a code span and a pipe would split the
# row, so both become plain text. Only the first non-empty line is kept, since
# git and gh both write a whole report.
one_line() {
  local text
  text="$(printf '%s\n' "$1" | tr -d '\r' | grep -m1 -v '^[[:space:]]*$' || true)"
  printf '%s' "${text:-unknown error}" | tr '\n\t' '  ' \
    | LC_ALL=C tr -cd '\40-\176' | cut -c1-300 \
    | sed 's/[[:space:]]*$//; s/|/\\|/g; s/`/'"'"'/g'
}

# A workflow command reads its message as an escaped string: a literal `%`
# there starts an escape sequence, so the runner would swallow it. one_line
# already removed line breaks, so only `%` needs encoding.
command_text() {
  printf '%s' "${1//%/%25}"
}
