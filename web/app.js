const DATA_DIR = "data";

const els = {
  source: document.querySelector("#sourceView"),
  tokenRows: document.querySelector("#tokenRows"),
  symbolRows: document.querySelector("#symbolRows"),
  astTree: document.querySelector("#astTree"),
  tac: document.querySelector("#tacView"),
  bytecode: document.querySelector("#bytecodeView"),
  reload: document.querySelector("#reloadBtn")
};

async function loadJson(path, fallback) {
  try {
    const res = await fetch(path, { cache: "no-store" });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    return await res.json();
  } catch (err) {
    console.warn(`Cannot load ${path}:`, err);
    return fallback;
  }
}

async function loadText(path, fallback = "") {
  try {
    const res = await fetch(path, { cache: "no-store" });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    return await res.text();
  } catch (err) {
    console.warn(`Cannot load ${path}:`, err);
    return fallback;
  }
}

function text(value) {
  return value === undefined || value === null ? "" : String(value);
}

function renderSource(source) {
  const lines = source.split(/\r?\n/);
  els.source.innerHTML = lines.map((line, index) => {
    const lineNo = index + 1;
    return `<span class="source-line" data-line="${lineNo}"><span class="line-no">${lineNo}</span><span class="line-text">${escapeHtml(line || " ")}</span></span>`;
  }).join("");
}

function highlightLine(line) {
  document.querySelectorAll(".source-line.is-active").forEach(el => el.classList.remove("is-active"));
  const target = document.querySelector(`.source-line[data-line="${line}"]`);
  if (target) {
    target.classList.add("is-active");
    target.scrollIntoView({ block: "center", behavior: "smooth" });
  }
}

function renderTokens(tokens) {
  els.tokenRows.innerHTML = tokens.map(token => `
    <tr>
      <td>${escapeHtml(text(token.kind))}</td>
      <td><code>${escapeHtml(text(token.lexeme))}</code></td>
      <td>${escapeHtml(text(token.line))}</td>
      <td>${escapeHtml(text(token.col))}</td>
      <td>${escapeHtml(text(token.length))}</td>
    </tr>
  `).join("");
}

function renderSymbols(symbols) {
  els.symbolRows.innerHTML = symbols.map(symbol => {
    const scope = symbol.scopeDepth ?? symbol.scope_depth ?? "";
    const declLine = symbol.declLine ?? symbol.decl_line ?? "";
    return `
      <tr>
        <td><code>${escapeHtml(text(symbol.name))}</code></td>
        <td>${escapeHtml(text(symbol.type))}</td>
        <td>${escapeHtml(text(scope))}</td>
        <td>${escapeHtml(text(declLine))}</td>
        <td>${symbol.initialized ? "yes" : "no"}</td>
        <td>${symbol.used ? "yes" : "no"}</td>
        <td>${escapeHtml(text(symbol.slot))}</td>
      </tr>
    `;
  }).join("");
}

function childNodes(node) {
  if (Array.isArray(node.children)) return node.children.filter(Boolean);
  return ["lhs", "rhs", "condition", "then", "else", "init", "step", "body"]
    .map(key => node[key])
    .filter(Boolean);
}

function renderAstNode(node) {
  if (!node) return document.createTextNode("");
  const children = childNodes(node);
  const details = document.createElement("details");
  details.open = children.length > 0;
  details.className = "ast-node";

  const summary = document.createElement("summary");
  const button = document.createElement("button");
  button.type = "button";
  button.className = "ast-label";
  button.dataset.line = node.line || "";
  button.innerHTML = `
    <span class="ast-kind">${escapeHtml(text(node.kind))}</span>
    <span class="ast-meta">#${escapeHtml(text(node.id))}</span>
    <span class="ast-meta">line ${escapeHtml(text(node.line))}</span>
    ${node.name ? `<span class="ast-name">${escapeHtml(node.name)}</span>` : ""}
    ${node.op ? `<span class="ast-op">${escapeHtml(node.op)}</span>` : ""}
  `;
  button.addEventListener("click", event => {
    event.stopPropagation();
    if (node.line) highlightLine(node.line);
  });
  summary.appendChild(button);
  details.appendChild(summary);

  const list = document.createElement("div");
  list.className = "ast-children";
  children.forEach(child => list.appendChild(renderAstNode(child)));
  details.appendChild(list);
  return details;
}

function renderAst(ast) {
  els.astTree.innerHTML = "";
  if (!ast || !ast.kind) {
    els.astTree.textContent = "AST data not available.";
    return;
  }
  els.astTree.appendChild(renderAstNode(ast));
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

async function loadDemo() {
  const [source, tokens, ast, symbols, tac, bytecode] = await Promise.all([
    loadText(`${DATA_DIR}/source.mc`, ""),
    loadJson(`${DATA_DIR}/tokens.json`, []),
    loadJson(`${DATA_DIR}/ast.json`, null),
    loadJson(`${DATA_DIR}/symbols.json`, []),
    loadText(`${DATA_DIR}/out.tac`, ""),
    loadText(`${DATA_DIR}/out.bc`, "")
  ]);

  renderSource(source);
  renderTokens(tokens);
  renderAst(ast);
  renderSymbols(symbols);
  els.tac.textContent = tac;
  els.bytecode.textContent = bytecode;
}

els.reload.addEventListener("click", loadDemo);
loadDemo();
