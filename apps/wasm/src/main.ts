import "./style.css";

import { el } from "./dom";
import { mountHome } from "./home";
import { mountPlayground } from "./playground";

const GITHUB_URL = "https://github.com/varn-org/varn";

function navLink(label: string, href: string, active: boolean): HTMLAnchorElement {
  const base = "rounded-md px-3 py-1.5 text-sm font-medium transition";
  const variant = active
    ? "bg-zinc-800 text-white"
    : "text-zinc-400 hover:bg-zinc-800/60 hover:text-white";
  const a = el("a", `${base} ${variant}`, label);
  a.href = href;
  if (href.startsWith("http")) {
    a.target = "_blank";
    a.rel = "noopener noreferrer";
  }
  return a;
}

function header(isPlayground: boolean): HTMLElement {
  const bar = el("header", "sticky top-0 z-10 border-b border-zinc-800/80 bg-zinc-950/80 backdrop-blur");
  const inner = el("div", "mx-auto flex max-w-5xl items-center justify-between gap-4 px-4 py-3 md:px-8");

  const brand = el("a", "flex items-center gap-2");
  brand.href = "#/";
  const logo = document.createElement("img");
  logo.src = "/logo-light.svg";
  logo.alt = "Varn";
  logo.className = "h-6 w-auto";
  brand.appendChild(logo);
  inner.appendChild(brand);

  const nav = el("nav", "flex items-center gap-1");
  nav.appendChild(navLink("Home", "#/", !isPlayground));
  nav.appendChild(navLink("Playground", "#/playground", isPlayground));
  nav.appendChild(navLink("GitHub", GITHUB_URL, false));
  inner.appendChild(nav);

  bar.appendChild(inner);
  return bar;
}

function footer(): HTMLElement {
  const f = el("footer", "border-t border-zinc-800/80 py-8 text-center text-xs text-zinc-500");
  const inner = el("div", "mx-auto max-w-5xl px-4");
  inner.appendChild(document.createTextNode("Varn · "));
  const a = el("a", "text-zinc-400 transition hover:text-white", "github.com/varn-org/varn");
  a.href = GITHUB_URL;
  a.target = "_blank";
  a.rel = "noopener noreferrer";
  inner.appendChild(a);
  f.appendChild(inner);
  return f;
}

let dispose: (() => void) | undefined;

function render(): void {
  const root = document.querySelector<HTMLDivElement>("#app");
  if (!root) {
    return;
  }

  if (dispose) {
    dispose();
    dispose = undefined;
  }

  root.className =
    "flex min-h-dvh flex-col bg-zinc-950 text-zinc-100 selection:bg-cyan-500/30 selection:text-cyan-50";
  root.replaceChildren();

  const isPlayground = location.hash.startsWith("#/playground");
  root.appendChild(header(isPlayground));

  const main = el("main", "flex-1");
  root.appendChild(main);

  if (isPlayground) {
    dispose = mountPlayground(main);
  } else {
    mountHome(main);
  }

  root.appendChild(footer());
  window.scrollTo(0, 0);
}

window.addEventListener("hashchange", render);
render();
