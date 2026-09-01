# 🕒 datetime

Parsing, formatting, and calendar arithmetic on instants — a single point in time held with
millisecond precision, internally as UTC, so instants always compare and subtract cleanly. Backed by
Howard Hinnant's date.h.

```lua
local datetime = require("datetime")
print(datetime.parse("2026-06-21T09:30:00-03:00"):iso())
```

## Capabilities

| Function | What it does |
|---|---|
| `datetime.now()` | An instant for the current moment. |
| `datetime.fromUnix(seconds)` | An instant at a Unix timestamp; fractional seconds are kept. |
| `datetime.fromMillis(ms)` | An instant at a Unix timestamp in milliseconds. |
| `datetime.parse(text)` | An instant from an ISO-8601 string (bare date, `T`/space separator, optional fraction and `Z`/`±hh:mm` offset); no offset is read as UTC. |
| `datetime.fromFields(t)` | An instant from a table of fields; `year`/`month`/`day` are required, the rest default to `0`, read at `offset` minutes then normalized to UTC. |
| `datetime.min(...)` / `datetime.max(...)` | The earliest or latest of the given instants. |
| `dt:unix()` / `dt:millis()` | The Unix timestamp in whole seconds, or in milliseconds. |
| `dt:iso(offset?)` | An ISO-8601 string — UTC with `Z`, or the wall clock at `offset` minutes ending `±hh:mm`; millis only when non-zero. |
| `dt:format(fmt?, offset?)` | A strftime-style string (`%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, `%A`, `%j`, …), in UTC or at a fixed `offset`. |
| `dt:fields(offset?)` | A table of `year`, `month`, `day`, `hour`, `minute`, `second`, `millis`, `weekday`, `yearday`, `offset` — for UTC or a fixed `offset`. |
| `dt:year()` / `dt:month()` / `dt:day()` / `dt:hour()` / `dt:minute()` / `dt:second()` | The individual UTC fields as numbers. |
| `dt:weekday()` / `dt:yearday()` | The ISO weekday (1=Monday … 7=Sunday), or the day of the year (1–366). |
| `dt:weekdayName()` / `dt:monthName()` | `"Monday"` … `"Sunday"`, or `"January"` … `"December"`. |
| `dt:isLeapYear()` / `dt:daysInMonth()` | Whether the year is a leap year, or the number of days in its month. |
| `dt:add(delta)` / `dt:subtract(delta)` | A new instant shifted by `years`, `months`, `weeks`, `days`, `hours`, `minutes`, `seconds`, `millis`; `years`/`months` are calendar-aware. |
| `dt:diff(other)` | Whole seconds from `other` to `dt`, positive when `dt` is later. |
| `dt:diffIn(other, unit)` | The signed difference in `"millis"`, `"seconds"`, `"minutes"`, `"hours"`, `"days"`, `"weeks"`, `"months"`, or `"years"`. |
| `dt:startOf(unit)` / `dt:endOf(unit)` | The first or last instant of the surrounding `"year"`, `"month"`, `"week"`, `"day"`, `"hour"`, `"minute"`, or `"second"`. |

Instants compare with the standard operators (`==`, `<`, `<=`, `>`, `>=`) and `tostring(dt)` returns
`dt:iso()`.

## Reference, examples, and tests

- Full reference: [docs/lua-api/datetime.md](../../docs/lua-api/datetime.md)
- Runnable examples: [lua/examples/](lua/examples/)
- Tests run in CI on Linux, macOS, and Windows: [lua/tests/](lua/tests/)
