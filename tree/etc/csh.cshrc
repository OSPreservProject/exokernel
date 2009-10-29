setenv TMPDIR /tmp
setenv TMP /tmp
setenv EMULATOR /bin/emulate
setenv ARCH "OpenBSD.i386"

set history=100
# for file name completion
set filec
# ignore control-D on shell prompt to avoid accidental logouts
set ignoreeof

set path=(/bin /usr/local/bin /usr/bin /sbin /usr/sbin /usr/cffs /benchmarks/bin . )
alias rfind 'find . -name "*.[cshS]" -exec fgrep \!* {} \; -print'

rehash
