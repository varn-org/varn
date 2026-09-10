# 📝 log

Covers `log.debug/info/warn/error(...)` — variadic, `tostring`-joined by tabs, one line per call, written through a backend (`stdout` / `spdlog` / `dummy`).

### Log injection (forging entries)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-001 | LF newline injection | CWE-117 | A `\n` in a value forges a new log line |
| LOG-002 | CR injection | CWE-117 | A `\r` rewrites the visible line |
| LOG-003 | CRLF injection | CWE-117 | A `\r\n` splits the entry |
| LOG-004 | Forged level tag | CWE-117 | Embedding `[error]` to fake severity |
| LOG-005 | Forged timestamp | CWE-117 | Embedding a fake timestamp prefix |
| LOG-006 | Forged structured field | CWE-117 | Injecting JSON/key=value into a structured sink |
| LOG-007 | Tab injection | CWE-117 | Extra tabs break tab-delimited parsing |
| LOG-008 | NUL byte in message | CWE-626 | A `\0` truncates or splits the line |
| LOG-009 | ANSI escape injection | CWE-150 | Terminal escape sequences in a value |
| LOG-010 | Cursor/clear-screen escapes | CWE-150 | The sequence `\x1b[2J` hides prior output |
| LOG-011 | Backspace/overstrike | CWE-150 | A `\b` rewrites characters |
| LOG-012 | Hyperlink escape (OSC 8) | CWE-150 | Terminal hyperlink injection |
| LOG-013 | Unicode bidi override | CWE-1007 | RTL override reorders text |
| LOG-014 | Multi-line value | CWE-117 | A value spanning many lines |
| LOG-015 | Injection per level (debug) | CWE-117 | CRLF at debug level |
| LOG-016 | Injection per level (info) | CWE-117 | CRLF at info level |
| LOG-017 | Injection per level (warn) | CWE-117 | CRLF at warn level |
| LOG-018 | Injection per level (error) | CWE-117 | CRLF at error level |
| LOG-019 | Request data → log | CWE-117 | User-controlled fields logged raw |
| LOG-020 | Header value → log | CWE-117 | Logging a raw header value |

### Format-string & rendering

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-021 | `%s`/`%n` in message | CWE-134 | The `printf` specifiers in a value |
| LOG-022 | spdlog `{}` substitution | CWE-134 | Placeholders `{}`/`{0}` interpreted by fmt |
| LOG-023 | `{}` with no args | CWE-134 | The fmt library throws on a missing argument |
| LOG-024 | `{:...}` format spec | CWE-134 | Abuse of the fmt format spec |
| LOG-025 | Brace escaping | CWE-134 | Handling of `{{`/`}}` |
| LOG-026 | `tostring` metamethod abuse | CWE-913 | A `__tostring` metamethod runs arbitrary Lua during logging |
| LOG-027 | `tostring` error in metamethod | CWE-755 | A throwing `__tostring` breaks logging |
| LOG-028 | Number formatting locale | CWE-697 | Locale changes numeric rendering |
| LOG-029 | Float NaN/Inf rendering | CWE-20 | Non-finite numbers rendered |
| LOG-030 | Huge number rendering | CWE-400 | Very long numeric expansion |

### Disclosure

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-031 | Secret in message | CWE-532 | Tokens/passwords logged |
| LOG-032 | Session id logged | CWE-532 | Session id in a log line |
| LOG-033 | JWT/secret in error log | CWE-532 | Error path logs the secret |
| LOG-034 | Full request body logged | CWE-532 | Sensitive body content |
| LOG-035 | Authorization header logged | CWE-532 | Credentials in logs |
| LOG-036 | Cookie logged | CWE-532 | Cookie values in logs |
| LOG-037 | PII logged | CWE-359 | Personal data in logs |
| LOG-038 | Internal path/stack logged | CWE-209 | Internals disclosed in logs |
| LOG-039 | Key material logged | CWE-532 | Crypto keys logged |
| LOG-040 | Log file world-readable | CWE-532 | Log sink permissions |

### Resource / DoS / backend

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-041 | Log-volume flood | CWE-400 | Forcing excessive logging fills disk/IO |
| LOG-042 | Huge single message | CWE-400 | A multi-GB log argument |
| LOG-043 | Many arguments | CWE-400 | Thousands of varargs joined |
| LOG-044 | Blocking sink on main loop | CWE-400 | A slow/blocking sink stalls the loop |
| LOG-045 | Disk-full on file sink | CWE-400 | Handling of `ENOSPC` |
| LOG-046 | Log rotation race | CWE-362 | Rotation while writing |
| LOG-047 | Unbounded buffer | CWE-400 | Queued log records grow unbounded |
| LOG-048 | Synchronous flush amplification | CWE-400 | Per-line flush thrashing |
| LOG-049 | Recursive logging | CWE-674 | A sink that logs while logging |
| LOG-050 | Sink init failure | CWE-703 | Backend init error handled |

### Concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-051 | Interleaved lines (race) | CWE-362 | Concurrent writers interleave output |
| LOG-052 | Torn multi-byte write | CWE-362 | Partial line on concurrent write |
| LOG-053 | Level set race | CWE-362 | Changing level concurrently |
| LOG-054 | Sink swap race | CWE-362 | Replacing the backend at runtime |
| LOG-055 | Thread-id correctness | CWE-20 | Wrong thread attribution |
| LOG-056 | Cross-thread `tostring` | CWE-362 | Calling `tostring` on a Lua value off the main loop |
| LOG-057 | Static-init order of logger | CWE-416 | Logging before/after backend lifetime |
| LOG-058 | Reentrant log in handler | CWE-674 | Logging from a log hook |
| LOG-059 | Atomic line guarantee | CWE-662 | A line is written atomically |
| LOG-060 | Shutdown flush | CWE-404 | Buffered records lost on exit |

### Binding / encoding / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-061 | NUL-truncated message | CWE-626 | The `lua_tostring` call truncates at NUL |
| LOG-062 | Length-aware logging | CWE-626 | Binary message length preserved |
| LOG-063 | Invalid UTF-8 message | CWE-176 | Raw bytes in a message |
| LOG-064 | Non-string args | CWE-20 | Table/function/userdata args |
| LOG-065 | `nil` args | CWE-20 | Rendering of `log.info(nil)` |
| LOG-066 | No args | CWE-20 | An empty line from `log.info()` |
| LOG-067 | Mixed-type args | CWE-20 | Numbers, bools, tables together |
| LOG-068 | Stack imbalance on error | CWE-664 | A logging error leaves a balanced stack |
| LOG-069 | Exception across boundary | CWE-248 | A backend throw is contained |
| LOG-070 | Argument fuzz | CWE-20 | Random/binary args never crash |
| LOG-071 | Huge tab-joined output | CWE-400 | Many large args joined |
| LOG-072 | Control bytes preserved/escaped | CWE-150 | Policy for raw control bytes |
| LOG-073 | Emoji/wide chars | CWE-20 | Wide-character rendering |
| LOG-074 | Overlong UTF-8 | CWE-176 | Overlong sequences |
| LOG-075 | Surrogate bytes | CWE-176 | Lone surrogate handling |

### Operational / integrity

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-076 | Log forging defeats audit | CWE-117 | Injected lines mislead an investigator |
| LOG-077 | Missing log on security event | CWE-778 | Auth failures not logged |
| LOG-078 | Over-logging hides signal | CWE-779 | Noise buries real events |
| LOG-079 | Timestamp accuracy | CWE-682 | Wrong/missing timestamps |
| LOG-080 | Timezone ambiguity | CWE-20 | Local vs UTC confusion |
| LOG-081 | Monotonic vs wall clock | CWE-682 | Reordering by clock |
| LOG-082 | Log injection → downstream SIEM | CWE-117 | Crafted line exploits the log parser |
| LOG-083 | CSV/JSON sink injection | CWE-1236 | Formula/`=`-prefixed field |
| LOG-084 | Field separator collision | CWE-117 | The tab delimiter appears in data |
| LOG-085 | Level filter bypass | CWE-693 | A way to force debug output in prod |
| LOG-086 | Sensitive default level | CWE-532 | Debug-by-default leaks data |
| LOG-087 | Redaction absent | CWE-532 | No secret redaction hook |
| LOG-088 | Backend selection at build | CWE-697 | The `stdout` vs `spdlog` vs `dummy` parity |
| LOG-089 | spdlog pattern injection | CWE-134 | Pattern controlled by input |
| LOG-090 | Async sink overflow policy | CWE-400 | Drop vs block under overflow |
| LOG-091 | Crash dumps in logs | CWE-532 | Core/stack content logged |
| LOG-092 | Color codes to file | CWE-150 | ANSI written to a file sink |
| LOG-093 | Reopen on SIGHUP race | CWE-362 | Log-reopen signal handling |
| LOG-094 | Long-line truncation | CWE-20 | Sink truncates silently |
| LOG-095 | Encoding of file sink | CWE-176 | Sink re-encodes bytes |
| LOG-096 | Buffered vs line-buffered | CWE-662 | Loss window on crash |
| LOG-097 | Multi-process log file | CWE-362 | Two processes write one file |
| LOG-098 | Permissions on rotation | CWE-276 | Rotated file too permissive |
| LOG-099 | Error in error path | CWE-755 | Logging the logging failure |
| LOG-100 | Fuzz across levels+sinks | CWE-20 | Random messages/levels/sinks never crash |

---

## Additional cases (deeper / documented)

### Injection into log pipelines (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-101 | Log4Shell-style lookup | CVE-2021-44228 | A `${jndi:...}` lookup reaches a downstream evaluator |
| LOG-102 | Logback JNDI eval | CVE-2021-42550 | Downstream config-driven eval |
| LOG-103 | Splunk SPL injection | CWE-117 | Crafted field abuses Splunk ingestion |
| LOG-104 | ELK/Kibana field injection | CWE-117 | Crafted JSON manipulates the index |
| LOG-105 | CSV-formula injection in export | CWE-1236 | A `=cmd|...` field opens in a spreadsheet |
| LOG-106 | Syslog priority injection | CWE-117 | Fake `<13>` PRI field |
| LOG-107 | Syslog message framing | CWE-117 | Octet-count/newline framing abuse |
| LOG-108 | journald field injection | CWE-117 | Forging a `KEY=value` field |
| LOG-109 | JSON-line log forging | CWE-117 | Injecting a full JSON record |
| LOG-110 | Key=value pair forging | CWE-117 | Extra delimiters create fake fields |
| LOG-111 | Timestamp spoof | CWE-117 | Embedding a fake timestamp |
| LOG-112 | Level/severity spoof | CWE-117 | Embedding `[ERROR]` |
| LOG-113 | Source/thread spoof | CWE-117 | Faking the origin field |
| LOG-114 | Multi-line stack-trace forge | CWE-117 | Injecting a fake traceback |
| LOG-115 | Audit-evasion via overwrite | CWE-117 | CR rewrites the visible line |

### Terminal / rendering (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-116 | ANSI color/escape injection | CWE-150 | Escape sequences in a value |
| LOG-117 | Clear-screen/cursor escapes | CWE-150 | The sequence `\x1b[2J` hides output |
| LOG-118 | OSC 8 hyperlink injection | CWE-150 | Terminal hyperlink in a log |
| LOG-119 | OSC 52 clipboard write | CWE-150 | Escape writes the clipboard |
| LOG-120 | Title-set escape | CWE-150 | The sequence `\x1b]0;` sets the terminal title |
| LOG-121 | Backspace overstrike | CWE-150 | A `\b` rewrites characters |
| LOG-122 | Bidi override (Trojan Source) | CWE-1007 | RLO reorders the line |
| LOG-123 | Zero-width chars | CWE-176 | Hidden characters |
| LOG-124 | Combining-char zalgo | CWE-400 | Rendering blowup |
| LOG-125 | Wide/emoji alignment break | CWE-20 | Breaks columnar parsing |

### Disclosure (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-126 | JWT secret logged | CWE-532 | Signing key in a log line |
| LOG-127 | Session id logged | CWE-532 | Session token logged |
| LOG-128 | Password in request log | CWE-532 | Credential in a logged body |
| LOG-129 | Authorization header logged | CWE-532 | Bearer token logged |
| LOG-130 | Cookie logged | CWE-532 | Cookie values logged |
| LOG-131 | API key logged | CWE-532 | Key in a query/log |
| LOG-132 | PII logged | CWE-359 | Personal data logged |
| LOG-133 | Card/financial data logged | CWE-532 | PCI-class disclosure |
| LOG-134 | Internal path/stack logged | CWE-209 | Internals disclosed |
| LOG-135 | Crypto key logged | CWE-532 | Key material logged |
| LOG-136 | Full SQL/query logged | CWE-532 | Sensitive query content |
| LOG-137 | Log file world-readable | CWE-532 | Sink permissions |
| LOG-138 | Debug-level default in prod | CWE-1295 | Verbose logging leaks data |
| LOG-139 | No redaction hook | CWE-532 | No way to mask secrets |
| LOG-140 | Crash dump in logs | CWE-528 | Core/stack content logged |

### Format string / metamethods (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-141 | `printf` `%s`/`%n` | CWE-134 | Format specifiers in a value |
| LOG-142 | spdlog/fmt `{}` substitution | CWE-134 | The fmt library interprets braces |
| LOG-143 | fmt missing-arg throw | CWE-134 | A `{}` with no argument |
| LOG-144 | fmt format-spec abuse | CWE-134 | A `{:...}` width/precision |
| LOG-145 | spdlog pattern injection | CWE-134 | Pattern controlled by input |
| LOG-146 | `__tostring` arbitrary code | CWE-913 | Metamethod runs during logging |
| LOG-147 | `__tostring` throws | CWE-755 | Throwing metamethod breaks logging |
| LOG-148 | Recursive `__tostring` | CWE-674 | Metamethod recurses |
| LOG-149 | Number-format locale | CWE-697 | Locale changes rendering |
| LOG-150 | NaN/Inf rendering | CWE-20 | Non-finite numbers |

### Resource / backend / concurrency (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-151 | Log-volume flood | CWE-779 | Excessive logging fills disk |
| LOG-152 | Huge single message | CWE-400 | Multi-GB argument |
| LOG-153 | Many arguments | CWE-400 | Thousands of varargs |
| LOG-154 | Blocking sink on main loop | CWE-400 | Slow sink stalls the loop |
| LOG-155 | Disk-full on file sink | CWE-400 | Handling of `ENOSPC` |
| LOG-156 | Rotation race | CWE-362 | Rotation while writing |
| LOG-157 | Unbounded async buffer | CWE-400 | Queued records grow |
| LOG-158 | Per-line flush thrash | CWE-400 | Sync flush amplification |
| LOG-159 | Recursive logging | CWE-674 | Sink logs while logging |
| LOG-160 | Sink init failure | CWE-703 | Backend init error |
| LOG-161 | Interleaved lines race | CWE-362 | Concurrent writers interleave |
| LOG-162 | Torn multi-byte write | CWE-362 | Partial line on concurrency |
| LOG-163 | Level-set race | CWE-362 | Level changed concurrently |
| LOG-164 | Sink-swap race | CWE-362 | Backend replaced at runtime |
| LOG-165 | Cross-thread `tostring` | CWE-362 | Coercing a Lua value off-thread |
| LOG-166 | Static-init order of logger | CWE-456 | Logging before backend init |
| LOG-167 | Shutdown flush loss | CWE-404 | Buffered records lost on exit |
| LOG-168 | Multi-process file write | CWE-362 | Two processes share a file |
| LOG-169 | SIGHUP reopen race | CWE-362 | Log-reopen handling |
| LOG-170 | Rotated-file permissions | CWE-276 | Rotated file too permissive |

### Binding / encoding / integrity / fuzz (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-171 | NUL-truncated message | CWE-626 | The `lua_tostring` call truncates |
| LOG-172 | Length-aware logging | CWE-626 | Binary message preserved |
| LOG-173 | Invalid UTF-8 message | CWE-176 | Raw bytes in a message |
| LOG-174 | Overlong UTF-8 | CWE-176 | Overlong sequences |
| LOG-175 | Surrogate bytes | CWE-176 | Lone surrogate |
| LOG-176 | Non-string args | CWE-20 | Table/function/userdata |
| LOG-177 | `nil`/no args | CWE-20 | Calls `log.info()`/`log.info(nil)` |
| LOG-178 | Mixed-type args | CWE-20 | Numbers/bools/tables together |
| LOG-179 | Stack imbalance on error | CWE-664 | Error leaves a balanced stack |
| LOG-180 | Exception across boundary | CWE-248 | Backend throw contained |
| LOG-181 | Argument fuzz | CWE-20 | Random/binary args never crash |
| LOG-182 | Field-separator collision | CWE-117 | Tab delimiter appears in data |
| LOG-183 | Missing security-event log | CWE-778 | Auth failures not logged |
| LOG-184 | Over-logging hides signal | CWE-779 | Noise buries real events |
| LOG-185 | Timestamp accuracy | CWE-682 | Wrong/missing timestamps |
| LOG-186 | Timezone ambiguity | CWE-20 | Local vs UTC |
| LOG-187 | Monotonic vs wall clock | CWE-682 | Reordering by clock |
| LOG-188 | Level-filter bypass | CWE-693 | Forcing debug in prod |
| LOG-189 | Backend selection parity | CWE-697 | The `stdout`/`spdlog`/`dummy` backends diverge |
| LOG-190 | Color codes to file sink | CWE-150 | ANSI written to a file |
| LOG-191 | Long-line truncation | CWE-20 | Sink truncates silently |
| LOG-192 | Encoding of file sink | CWE-176 | Sink re-encodes bytes |
| LOG-193 | Buffered loss window | CWE-662 | Crash loses buffered logs |
| LOG-194 | Error-in-error-path | CWE-755 | Logging the logging failure |
| LOG-195 | Reentrant log in a hook | CWE-674 | Logging from a log hook |
| LOG-196 | Atomic-line guarantee | CWE-662 | A line written atomically |
| LOG-197 | Thread-id attribution | CWE-20 | Wrong thread in the line |
| LOG-198 | Async overflow policy | CWE-400 | Drop vs block under overflow |
| LOG-199 | Differential sink behavior | CWE-697 | Sinks disagree on output |
| LOG-200 | Fuzz across levels + sinks | CWE-20 | Random messages/levels/sinks never crash |
