import { el } from "./dom";

const GITHUB_URL = "https://github.com/varn-org/varn";

const MODULES: ReadonlyArray<{ icon: string; name: string; blurb: string }> = [
  { icon: "🌐", name: "http", blurb: "Web server and client, routing, middleware, WebSockets, SSE, static files" },
  { icon: "🔌", name: "socket", blurb: "TCP, TLS, UDP, and unix-domain connections" },
  { icon: "⏳", name: "async", blurb: "Background tasks, timers, promises, and combinators" },
  { icon: "📁", name: "fs", blurb: "Read, write, stream, stat, and list files" },
  { icon: "🔐", name: "crypto", blurb: "Hashing, HMAC, passwords, encryption, encoding, UUIDs, random" },
  { icon: "🧾", name: "json", blurb: "Encode and decode JSON with a full Lua mapping" },
  { icon: "🧬", name: "xml", blurb: "Encode and decode XML with a full Lua mapping" },
  { icon: "🗜️", name: "zip", blurb: "Create, extract, and list archives" },
  { icon: "🕒", name: "datetime", blurb: "Parse, format, and do calendar arithmetic on instants" },
  { icon: "⚙️", name: "process", blurb: "Run commands and read the environment and arguments" },
  { icon: "🧩", name: "ffi", blurb: "Call functions from native libraries" },
  { icon: "📝", name: "log", blurb: "Leveled, structured logging to the console or a file" },
];

const COMPONENTS: ReadonlyArray<{ icon: string; name: string; blurb: string }> = [
  { icon: "🤖", name: "ai", blurb: "Text, images, and audio across OpenAI, Claude, Gemini, ElevenLabs, and OpenAI-compatible providers" },
  { icon: "⏱", name: "scheduler", blurb: "Durable background jobs, immediate, scheduled, or recurring, with retries and crash recovery" },
  { icon: "🗄️", name: "vdo", blurb: "PDO-style database access over SQLite, MySQL, and PostgreSQL" },
  { icon: "⚡", name: "redis", blurb: "Redis client over a single connection with auto-pipelining" },
  { icon: "🪣", name: "pool", blurb: "Async resource pooling for clients and connections" },
];

const PERF: ReadonlyArray<{ route: string; varn: string; node: string; python: string }> = [
  { route: "HTTP plaintext", varn: "131k", node: "50k", python: "48k" },
  { route: "HTTP json", varn: "109k", node: "49k", python: "44k" },
  { route: "MySQL select", varn: "13.5k", node: "10.6k", python: "10.7k" },
  { route: "Redis incr", varn: "44.4k", node: "35.0k", python: "15.7k" },
];

const QUICKSTART = `local http = require("http")

local server = http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end)

server:listen(3000)`;

function linkButton(label: string, href: string, primary: boolean): HTMLAnchorElement {
  const base =
    "inline-flex items-center justify-center gap-2 rounded-lg px-5 py-2.5 text-sm font-semibold transition focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2";
  const variant = primary
    ? "bg-cyan-500 text-zinc-950 shadow-lg shadow-cyan-500/25 hover:bg-cyan-400 focus-visible:outline-cyan-300"
    : "border border-zinc-700 bg-zinc-900 text-zinc-100 hover:border-zinc-500 hover:bg-zinc-800 focus-visible:outline-zinc-400";
  const a = el("a", `${base} ${variant}`, label);
  a.href = href;
  if (href.startsWith("http")) {
    a.target = "_blank";
    a.rel = "noopener noreferrer";
  }
  return a;
}

function sectionTitle(text: string): HTMLElement {
  return el("h2", "text-xs font-semibold uppercase tracking-[0.2em] text-cyan-400/90", text);
}

export function mountHome(root: HTMLElement): void {
  const shell = el("div", "mx-auto flex max-w-5xl flex-col gap-16 px-4 py-12 md:px-8 md:py-16");

  // hero
  const hero = el("section", "flex flex-col items-center gap-6 text-center");
  const logo = document.createElement("img");
  logo.src = "/logo-light.svg";
  logo.alt = "Varn";
  logo.className = "h-20 w-auto md:h-24";
  hero.appendChild(logo);
  hero.appendChild(
    el(
      "h1",
      "max-w-3xl text-3xl font-semibold leading-tight tracking-tight text-white md:text-5xl",
      "Lua everywhere.",
    ),
  );
  hero.appendChild(
    el(
      "p",
      "max-w-2xl text-base leading-relaxed text-zinc-400 md:text-lg",
      "Write the whole application once in Lua and run that same code on a computer, a phone, or in the browser — on a fast C++ core that already carries the web layer, networking, background work, storage and crypto, so there is nothing to assemble.",
    ),
  );
  const heroCtas = el("div", "mt-2 flex flex-wrap items-center justify-center gap-3");
  heroCtas.appendChild(linkButton("Open the playground", "#/playground", true));
  heroCtas.appendChild(linkButton("View on GitHub", GITHUB_URL, false));
  hero.appendChild(heroCtas);
  shell.appendChild(hero);

  // quickstart
  const quick = el("section", "flex flex-col gap-4");
  quick.appendChild(sectionTitle("Quickstart"));
  quick.appendChild(
    el("p", "max-w-2xl text-sm leading-relaxed text-zinc-400", "A working web server in a few lines — the same script runs everywhere."),
  );
  const code = el(
    "pre",
    "overflow-auto rounded-2xl border border-zinc-800/80 bg-zinc-950/70 p-5 font-mono text-sm leading-relaxed text-cyan-50 shadow-xl shadow-black/40",
  );
  code.textContent = QUICKSTART;
  quick.appendChild(code);
  shell.appendChild(quick);

  // modules
  const modules = el("section", "flex flex-col gap-4");
  modules.appendChild(sectionTitle("Batteries included"));
  modules.appendChild(
    el(
      "p",
      "max-w-2xl text-sm leading-relaxed text-zinc-400",
      "Every module is independent and used through require, so a script pulls in only what it needs.",
    ),
  );
  const grid = el("div", "grid gap-3 sm:grid-cols-2 lg:grid-cols-3");
  for (const m of MODULES) {
    const card = el(
      "div",
      "flex flex-col gap-1 rounded-xl border border-zinc-800/80 bg-zinc-900/40 p-4 transition hover:border-zinc-700 hover:bg-zinc-900/70",
    );
    const head = el("div", "flex items-center gap-2");
    head.appendChild(el("span", "text-lg", m.icon));
    head.appendChild(el("span", "font-mono text-sm font-semibold text-cyan-300", m.name));
    card.appendChild(head);
    card.appendChild(el("p", "text-sm leading-relaxed text-zinc-400", m.blurb));
    grid.appendChild(card);
  }
  modules.appendChild(grid);
  shell.appendChild(modules);

  // components
  const components = el("section", "flex flex-col gap-4");
  components.appendChild(sectionTitle("Components"));
  components.appendChild(
    el(
      "p",
      "max-w-2xl text-sm leading-relaxed text-zinc-400",
      "Higher-level libraries written in Lua on top of the core, loaded with require. These run server-side on the native runtime.",
    ),
  );
  const componentsGrid = el("div", "grid gap-3 sm:grid-cols-2 lg:grid-cols-3");
  for (const c of COMPONENTS) {
    const card = el(
      "div",
      "flex flex-col gap-1 rounded-xl border border-zinc-800/80 bg-zinc-900/40 p-4 transition hover:border-zinc-700 hover:bg-zinc-900/70",
    );
    const head = el("div", "flex items-center gap-2");
    head.appendChild(el("span", "text-lg", c.icon));
    head.appendChild(el("span", "font-mono text-sm font-semibold text-cyan-300", c.name));
    card.appendChild(head);
    card.appendChild(el("p", "text-sm leading-relaxed text-zinc-400", c.blurb));
    componentsGrid.appendChild(card);
  }
  components.appendChild(componentsGrid);
  shell.appendChild(components);

  // performance
  const perf = el("section", "flex flex-col gap-4");
  perf.appendChild(sectionTitle("Performance"));
  perf.appendChild(
    el(
      "p",
      "max-w-2xl text-sm leading-relaxed text-zinc-400",
      "Requests per second on one core (Linux), no framework on any side — against raw Node http and a raw ASGI app on uvicorn. Varn leads every route, database and cache included, with a fraction of the tail latency.",
    ),
  );
  const tableWrap = el("div", "overflow-hidden rounded-2xl border border-zinc-800/80");
  const table = el("table", "w-full text-left text-sm");
  const thead = el("thead", "bg-zinc-900/60 text-zinc-400");
  const htr = el("tr", "");
  for (const [i, h] of ["Scenario", "Varn", "Node", "Python"].entries()) {
    htr.appendChild(el("th", `px-4 py-3 font-medium ${i === 0 ? "" : "text-right"}`, h));
  }
  thead.appendChild(htr);
  table.appendChild(thead);
  const tbody = el("tbody", "divide-y divide-zinc-800/80");
  for (const row of PERF) {
    const tr = el("tr", "text-zinc-300");
    tr.appendChild(el("td", "px-4 py-3", row.route));
    tr.appendChild(el("td", "px-4 py-3 text-right font-semibold text-cyan-300", row.varn));
    tr.appendChild(el("td", "px-4 py-3 text-right text-zinc-400", row.node));
    tr.appendChild(el("td", "px-4 py-3 text-right text-zinc-400", row.python));
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  tableWrap.appendChild(table);
  perf.appendChild(tableWrap);
  shell.appendChild(perf);

  // closing cta
  const closing = el(
    "section",
    "flex flex-col items-center gap-4 rounded-2xl border border-zinc-800/80 bg-zinc-900/40 px-6 py-10 text-center",
  );
  closing.appendChild(el("h2", "text-2xl font-semibold tracking-tight text-white", "Try it right here"));
  closing.appendChild(
    el(
      "p",
      "max-w-xl text-sm leading-relaxed text-zinc-400",
      "The playground runs the real Varn engine compiled to WebAssembly, in your browser. Pick a module and run it.",
    ),
  );
  const closingCtas = el("div", "flex flex-wrap items-center justify-center gap-3");
  closingCtas.appendChild(linkButton("Open the playground", "#/playground", true));
  closingCtas.appendChild(linkButton("View on GitHub", GITHUB_URL, false));
  closing.appendChild(closingCtas);
  shell.appendChild(closing);

  root.appendChild(shell);
}
