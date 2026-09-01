# 📝 log

`log.debug/info/warn/error(...)` — variadic, `tostring`-joined by tabs, one line per call,
written through a backend (stdout / spdlog / dummy).

### Log injection (forging entries)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-001 | LF newline injection | CWE-117 | `\n` in a value forges a new log line |
| LOG-002 | CR injection | CWE-117 | `\r` rewrites the visible line |
| LOG-003 | CRLF injection | CWE-117 | `\r\n` splits the entry |
| LOG-004 | Forged level tag | CWE-117 | embedding `[error]` to fake severity |
| LOG-005 | Forged timestamp | CWE-117 | embedding a fake timestamp prefix |
| LOG-006 | Forged structured field | CWE-117 | injecting JSON/key=value into a structured sink |
| LOG-007 | Tab injection | CWE-117 | extra tabs break tab-delimited parsing |
| LOG-008 | NUL byte in message | CWE-626 | `\0` truncates or splits the line |
| LOG-009 | ANSI escape injection | CWE-150 | terminal escape sequences in a value |
| LOG-010 | Cursor/clear-screen escapes | CWE-150 | `\x1b[2J` hides prior output |
| LOG-011 | Backspace/overstrike | CWE-150 | `\b` rewrites characters |
| LOG-012 | Hyperlink escape (OSC 8) | CWE-150 | terminal hyperlink injection |
| LOG-013 | Unicode bidi override | CWE-1007 | RTL override reorders text |
| LOG-014 | Multi-line value | CWE-117 | a value spanning many lines |
| LOG-015 | Injection per level (debug) | CWE-117 | CRLF at debug level |
| LOG-016 | Injection per level (info) | CWE-117 | CRLF at info level |
| LOG-017 | Injection per level (warn) | CWE-117 | CRLF at warn level |
| LOG-018 | Injection per level (error) | CWE-117 | CRLF at error level |
| LOG-019 | Request data → log | CWE-117 | user-controlled fields logged raw |
| LOG-020 | Header value → log | CWE-117 | logging a raw header value |

### Format-string & rendering

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-021 | `%s`/`%n` in message | CWE-134 | printf specifiers in a value |
| LOG-022 | spdlog `{}` substitution | CWE-134 | `{}`/`{0}` interpreted by fmt |
| LOG-023 | `{}` with no args | CWE-134 | fmt throws on a missing argument |
| LOG-024 | `{:...}` format spec | CWE-134 | fmt format-spec abuse |
| LOG-025 | Brace escaping | CWE-134 | `{{`/`}}` handling |
| LOG-026 | `tostring` metamethod abuse | CWE-913 | `__tostring` runs arbitrary Lua during logging |
| LOG-027 | `tostring` error in metamethod | CWE-755 | a throwing `__tostring` breaks logging |
| LOG-028 | Number formatting locale | CWE-697 | locale changes numeric rendering |
| LOG-029 | Float NaN/Inf rendering | CWE-20 | non-finite numbers rendered |
| LOG-030 | Huge number rendering | CWE-400 | very long numeric expansion |

### Disclosure

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-031 | Secret in message | CWE-532 | tokens/passwords logged |
| LOG-032 | Session id logged | CWE-532 | session id in a log line |
| LOG-033 | JWT/secret in error log | CWE-532 | error path logs the secret |
| LOG-034 | Full request body logged | CWE-532 | sensitive body content |
| LOG-035 | Authorization header logged | CWE-532 | credentials in logs |
| LOG-036 | Cookie logged | CWE-532 | cookie values in logs |
| LOG-037 | PII logged | CWE-359 | personal data in logs |
| LOG-038 | Internal path/stack logged | CWE-209 | internals disclosed in logs |
| LOG-039 | Key material logged | CWE-532 | crypto keys logged |
| LOG-040 | Log file world-readable | CWE-532 | log sink permissions |

### Resource / DoS / backend

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-041 | Log-volume flood | CWE-400 | forcing excessive logging fills disk/IO |
| LOG-042 | Huge single message | CWE-400 | a multi-GB log argument |
| LOG-043 | Many arguments | CWE-400 | thousands of varargs joined |
| LOG-044 | Blocking sink on main loop | CWE-400 | a slow/blocking sink stalls the loop |
| LOG-045 | Disk-full on file sink | CWE-400 | ENOSPC handling |
| LOG-046 | Log rotation race | CWE-362 | rotation while writing |
| LOG-047 | Unbounded buffer | CWE-400 | queued log records grow unbounded |
| LOG-048 | Synchronous flush amplification | CWE-400 | per-line flush thrashing |
| LOG-049 | Recursive logging | CWE-674 | a sink that logs while logging |
| LOG-050 | Sink init failure | CWE-703 | backend init error handled |

### Concurrency

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-051 | Interleaved lines (race) | CWE-362 | concurrent writers interleave output |
| LOG-052 | Torn multi-byte write | CWE-362 | partial line on concurrent write |
| LOG-053 | Level set race | CWE-362 | changing level concurrently |
| LOG-054 | Sink swap race | CWE-362 | replacing the backend at runtime |
| LOG-055 | Thread-id correctness | CWE-20 | wrong thread attribution |
| LOG-056 | Cross-thread tostring | CWE-362 | `tostring` of a Lua value off the main loop |
| LOG-057 | Static-init order of logger | CWE-416 | logging before/after backend lifetime |
| LOG-058 | Reentrant log in handler | CWE-674 | logging from a log hook |
| LOG-059 | Atomic line guarantee | CWE-662 | a line is written atomically |
| LOG-060 | Shutdown flush | CWE-404 | buffered records lost on exit |

### Binding / encoding / fuzz

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-061 | NUL-truncated message | CWE-626 | `lua_tostring` truncates at NUL |
| LOG-062 | Length-aware logging | CWE-626 | binary message length preserved |
| LOG-063 | Invalid UTF-8 message | CWE-176 | raw bytes in a message |
| LOG-064 | Non-string args | CWE-20 | table/function/userdata args |
| LOG-065 | nil args | CWE-20 | `log.info(nil)` rendering |
| LOG-066 | No args | CWE-20 | `log.info()` empty line |
| LOG-067 | Mixed-type args | CWE-20 | numbers, bools, tables together |
| LOG-068 | Stack imbalance on error | CWE-664 | a logging error leaves a balanced stack |
| LOG-069 | Exception across boundary | CWE-248 | a backend throw is contained |
| LOG-070 | Argument fuzz | CWE-20 | random/binary args never crash |
| LOG-071 | Huge tab-joined output | CWE-400 | many large args joined |
| LOG-072 | Control bytes preserved/escaped | CWE-150 | policy for raw control bytes |
| LOG-073 | Emoji/wide chars | CWE-20 | wide-character rendering |
| LOG-074 | Overlong UTF-8 | CWE-176 | overlong sequences |
| LOG-075 | Surrogate bytes | CWE-176 | lone surrogate handling |

### Operational / integrity

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-076 | Log forging defeats audit | CWE-117 | injected lines mislead an investigator |
| LOG-077 | Missing log on security event | CWE-778 | auth failures not logged |
| LOG-078 | Over-logging hides signal | CWE-779 | noise buries real events |
| LOG-079 | Timestamp accuracy | CWE-682 | wrong/missing timestamps |
| LOG-080 | Timezone ambiguity | CWE-20 | local vs UTC confusion |
| LOG-081 | Monotonic vs wall clock | CWE-682 | reordering by clock |
| LOG-082 | Log injection → downstream SIEM | CWE-117 | crafted line exploits the log parser |
| LOG-083 | CSV/JSON sink injection | CWE-1236 | formula/`=`-prefixed field |
| LOG-084 | Field separator collision | CWE-117 | the tab delimiter appears in data |
| LOG-085 | Level filter bypass | CWE-693 | a way to force debug output in prod |
| LOG-086 | Sensitive default level | CWE-532 | debug-by-default leaks data |
| LOG-087 | Redaction absent | CWE-532 | no secret redaction hook |
| LOG-088 | Backend selection at build | CWE-697 | stdout vs spdvs dummy parity |
| LOG-089 | spdlog pattern injection | CWE-134 | pattern controlled by input |
| LOG-090 | Async sink overflow policy | CWE-400 | drop vs block under overflow |
| LOG-091 | Crash dumps in logs | CWE-532 | core/stack content logged |
| LOG-092 | Color codes to file | CWE-150 | ANSI written to a file sink |
| LOG-093 | Reopen on SIGHUP race | CWE-362 | log-reopen signal handling |
| LOG-094 | Long-line truncation | CWE-20 | sink truncates silently |
| LOG-095 | Encoding of file sink | CWE-176 | sink re-encodes bytes |
| LOG-096 | Buffered vs line-buffered | CWE-662 | loss window on crash |
| LOG-097 | Multi-process log file | CWE-362 | two processes write one file |
| LOG-098 | Permissions on rotation | CWE-276 | rotated file too permissive |
| LOG-099 | Error in error path | CWE-755 | logging the logging failure |
| LOG-100 | Fuzz across levels+sinks | CWE-20 | random messages/levels/sinks never crash |

---

## Additional cases (deeper / documented)

### Injection into log pipelines (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-101 | Log4Shell-style lookup | CVE-2021-44228 | `${jndi:...}` reaches a downstream evaluator |
| LOG-102 | Logback JNDI eval | CVE-2021-42550 | downstream config-driven eval |
| LOG-103 | Splunk SPL injection | CWE-117 | crafted field abuses Splunk ingestion |
| LOG-104 | ELK/Kibana field injection | CWE-117 | crafted JSON manipulates the index |
| LOG-105 | CSV-formula injection in export | CWE-1236 | `=cmd|...` opens in a spreadsheet |
| LOG-106 | Syslog priority injection | CWE-117 | fake `<13>` PRI field |
| LOG-107 | Syslog message framing | CWE-117 | octet-count/newline framing abuse |
| LOG-108 | journald field injection | CWE-117 | `KEY=value` field forging |
| LOG-109 | JSON-line log forging | CWE-117 | injecting a full JSON record |
| LOG-110 | Key=value pair forging | CWE-117 | extra delimiters create fake fields |
| LOG-111 | Timestamp spoof | CWE-117 | embedding a fake timestamp |
| LOG-112 | Level/severity spoof | CWE-117 | embedding `[ERROR]` |
| LOG-113 | Source/thread spoof | CWE-117 | faking the origin field |
| LOG-114 | Multi-line stack-trace forge | CWE-117 | injecting a fake traceback |
| LOG-115 | Audit-evasion via overwrite | CWE-117 | CR rewrites the visible line |

### Terminal / rendering (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-116 | ANSI color/escape injection | CWE-150 | escape sequences in a value |
| LOG-117 | Clear-screen/cursor escapes | CWE-150 | `\x1b[2J` hides output |
| LOG-118 | OSC 8 hyperlink injection | CWE-150 | terminal hyperlink in a log |
| LOG-119 | OSC 52 clipboard write | CWE-150 | escape writes the clipboard |
| LOG-120 | Title-set escape | CWE-150 | `\x1b]0;` sets the terminal title |
| LOG-121 | Backspace overstrike | CWE-150 | `\b` rewrites characters |
| LOG-122 | Bidi override (Trojan Source) | CWE-1007 | RLO reorders the line |
| LOG-123 | Zero-width chars | CWE-176 | hidden characters |
| LOG-124 | Combining-char zalgo | CWE-400 | rendering blowup |
| LOG-125 | Wide/emoji alignment break | CWE-20 | breaks columnar parsing |

### Disclosure (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-126 | JWT secret logged | CWE-532 | signing key in a log line |
| LOG-127 | Session id logged | CWE-532 | session token logged |
| LOG-128 | Password in request log | CWE-532 | credential in a logged body |
| LOG-129 | Authorization header logged | CWE-532 | bearer token logged |
| LOG-130 | Cookie logged | CWE-532 | cookie values logged |
| LOG-131 | API key logged | CWE-532 | key in a query/log |
| LOG-132 | PII logged | CWE-359 | personal data logged |
| LOG-133 | Card/financial data logged | CWE-532 | PCI-class disclosure |
| LOG-134 | Internal path/stack logged | CWE-209 | internals disclosed |
| LOG-135 | Crypto key logged | CWE-532 | key material logged |
| LOG-136 | Full SQL/query logged | CWE-532 | sensitive query content |
| LOG-137 | Log file world-readable | CWE-532 | sink permissions |
| LOG-138 | Debug-level default in prod | CWE-1295 | verbose logging leaks data |
| LOG-139 | No redaction hook | CWE-532 | no way to mask secrets |
| LOG-140 | Crash dump in logs | CWE-528 | core/stack content logged |

### Format string / metamethods (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-141 | printf `%s`/`%n` | CWE-134 | format specifiers in a value |
| LOG-142 | spdlog/fmt `{}` substitution | CWE-134 | fmt interprets braces |
| LOG-143 | fmt missing-arg throw | CWE-134 | `{}` with no argument |
| LOG-144 | fmt format-spec abuse | CWE-134 | `{:...}` width/precision |
| LOG-145 | spdlog pattern injection | CWE-134 | pattern controlled by input |
| LOG-146 | `__tostring` arbitrary code | CWE-913 | metamethod runs during logging |
| LOG-147 | `__tostring` throws | CWE-755 | throwing metamethod breaks logging |
| LOG-148 | Recursive `__tostring` | CWE-674 | metamethod recurses |
| LOG-149 | Number-format locale | CWE-697 | locale changes rendering |
| LOG-150 | NaN/Inf rendering | CWE-20 | non-finite numbers |

### Resource / backend / concurrency (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-151 | Log-volume flood | CWE-779 | excessive logging fills disk |
| LOG-152 | Huge single message | CWE-400 | multi-GB argument |
| LOG-153 | Many arguments | CWE-400 | thousands of varargs |
| LOG-154 | Blocking sink on main loop | CWE-400 | slow sink stalls the loop |
| LOG-155 | Disk-full on file sink | CWE-400 | ENOSPC handling |
| LOG-156 | Rotation race | CWE-362 | rotation while writing |
| LOG-157 | Unbounded async buffer | CWE-400 | queued records grow |
| LOG-158 | Per-line flush thrash | CWE-400 | sync flush amplification |
| LOG-159 | Recursive logging | CWE-674 | sink logs while logging |
| LOG-160 | Sink init failure | CWE-703 | backend init error |
| LOG-161 | Interleaved lines race | CWE-362 | concurrent writers interleave |
| LOG-162 | Torn multi-byte write | CWE-362 | partial line on concurrency |
| LOG-163 | Level-set race | CWE-362 | level changed concurrently |
| LOG-164 | Sink-swap race | CWE-362 | backend replaced at runtime |
| LOG-165 | Cross-thread `tostring` | CWE-362 | coercing a Lua value off-thread |
| LOG-166 | Static-init order of logger | CWE-456 | logging before backend init |
| LOG-167 | Shutdown flush loss | CWE-404 | buffered records lost on exit |
| LOG-168 | Multi-process file write | CWE-362 | two processes share a file |
| LOG-169 | SIGHUP reopen race | CWE-362 | log-reopen handling |
| LOG-170 | Rotated-file permissions | CWE-276 | rotated file too permissive |

### Binding / encoding / integrity / fuzz (documented)

| ID | Name | Class | Exploit |
|----|------|-------|---------|
| LOG-171 | NUL-truncated message | CWE-626 | `lua_tostring` truncates |
| LOG-172 | Length-aware logging | CWE-626 | binary message preserved |
| LOG-173 | Invalid UTF-8 message | CWE-176 | raw bytes in a message |
| LOG-174 | Overlong UTF-8 | CWE-176 | overlong sequences |
| LOG-175 | Surrogate bytes | CWE-176 | lone surrogate |
| LOG-176 | Non-string args | CWE-20 | table/function/userdata |
| LOG-177 | nil/no args | CWE-20 | `log.info()`/`log.info(nil)` |
| LOG-178 | Mixed-type args | CWE-20 | numbers/bools/tables together |
| LOG-179 | Stack imbalance on error | CWE-664 | error leaves a balanced stack |
| LOG-180 | Exception across boundary | CWE-248 | backend throw contained |
| LOG-181 | Argument fuzz | CWE-20 | random/binary args never crash |
| LOG-182 | Field-separator collision | CWE-117 | tab delimiter appears in data |
| LOG-183 | Missing security-event log | CWE-778 | auth failures not logged |
| LOG-184 | Over-logging hides signal | CWE-779 | noise buries real events |
| LOG-185 | Timestamp accuracy | CWE-682 | wrong/missing timestamps |
| LOG-186 | Timezone ambiguity | CWE-20 | local vs UTC |
| LOG-187 | Monotonic vs wall clock | CWE-682 | reordering by clock |
| LOG-188 | Level-filter bypass | CWE-693 | forcing debug in prod |
| LOG-189 | Backend selection parity | CWE-697 | stdout/spdlog/dummy diverge |
| LOG-190 | Color codes to file sink | CWE-150 | ANSI written to a file |
| LOG-191 | Long-line truncation | CWE-20 | sink truncates silently |
| LOG-192 | Encoding of file sink | CWE-176 | sink re-encodes bytes |
| LOG-193 | Buffered loss window | CWE-662 | crash loses buffered logs |
| LOG-194 | Error-in-error-path | CWE-755 | logging the logging failure |
| LOG-195 | Reentrant log in a hook | CWE-674 | logging from a log hook |
| LOG-196 | Atomic-line guarantee | CWE-662 | a line written atomically |
| LOG-197 | Thread-id attribution | CWE-20 | wrong thread in the line |
| LOG-198 | Async overflow policy | CWE-400 | drop vs block under overflow |
| LOG-199 | Differential sink behavior | CWE-697 | sinks disagree on output |
| LOG-200 | Fuzz across levels + sinks | CWE-20 | random messages/levels/sinks never crash |
