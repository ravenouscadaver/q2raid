import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, "../..");
const port = Number(process.env.PORT || 4173);
const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".css", "text/css; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".png", "image/png"],
  [".jpg", "image/jpeg"],
  [".jpeg", "image/jpeg"]
]);

function resolveRequest(urlPath) {
  const decoded = decodeURIComponent(urlPath.split("?")[0]);
  const relative = decoded.replace(/^\/+/, "");
  const candidate = path.resolve(repoRoot, relative);
  if (candidate !== repoRoot && !candidate.startsWith(repoRoot + path.sep))
    return null;
  return candidate;
}

const server = createServer(async (req, res) => {
  let pathname = req.url || "/";
  if (pathname === "/" || pathname === "/tools/terminal-harness" || pathname === "/tools/terminal-harness/")
    pathname = "/tools/terminal-harness/index.html";

  const filePath = resolveRequest(pathname);
  if (!filePath) {
    res.writeHead(403).end("Forbidden");
    return;
  }

  try {
    if (!(await stat(filePath)).isFile())
      throw new Error("not a file");
    const data = await readFile(filePath);
    res.writeHead(200, {
      "Content-Type": contentTypes.get(path.extname(filePath).toLowerCase()) || "application/octet-stream",
      "Cache-Control": "no-store"
    });
    res.end(data);
  } catch {
    res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" });
    res.end(`Not found: ${pathname}`);
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Q2Raid terminal harness: http://127.0.0.1:${port}/tools/terminal-harness/`);
  console.log("Serving the canonical repository tree; terminal JSON/assets reload without rebuilding TypeScript.");
});
