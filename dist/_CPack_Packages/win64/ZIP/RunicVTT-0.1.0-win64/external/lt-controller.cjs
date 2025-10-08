#!/usr/bin/env node
'use strict';

const path = require('path');
const fs = require('fs');
const { createRequire } = require('module');

function log(o) { try { process.stdout.write(JSON.stringify(o) + "\n"); } catch { } }

// Boot info
log({ event: 'boot', cwd: process.cwd(), argv: process.argv, node: process.version });

// Anchor require to your shipped localtunnel:
// external/localtunnel/node_modules/localtunnel/package.json
const ltPkgJson = path.join(__dirname, 'localtunnel', 'node_modules', 'localtunnel', 'package.json');
if (!fs.existsSync(ltPkgJson)) {
    log({ event: 'require_err', message: 'package.json not found: ' + ltPkgJson });
    process.exit(1);
}
const requireLT = createRequire(ltPkgJson);

let lt, ltPkg;
try {
    lt = requireLT('localtunnel');     // the library
    ltPkg = requireLT('./package.json');  // its own package.json (to log version)
    log({ event: 'require_ok', lt_version: ltPkg && ltPkg.version });
} catch (e) {
    log({ event: 'require_err', message: e && e.message ? e.message : String(e) });
    process.exit(1);
}

// ---- manual arg parse (no deps) ----
const args = process.argv.slice(2);
let port = null, subdomain = null;
for (let i = 0; i < args.length; ++i) {
    if (args[i] === '--port') port = parseInt(args[++i], 10);
    else if (args[i] === '--subdomain') subdomain = args[++i];
}
if (!port) {
    log({ event: 'error', message: 'Missing --port' });
    process.exit(1);
}

// ---- Start localtunnel using callback API (no Promises) ----
let tunnel = null;
let started = false;
try {
    const maybe = lt(port, { subdomain }, function onReady(err, t) {
        if (started) return; // guard against duplicate cb
        started = true;

        if (err) {
            log({ event: 'error', message: err && err.message ? err.message : String(err) });
            process.exit(1);
            return;
        }

        tunnel = t || maybe;
        if (!tunnel) {
            log({ event: 'error', message: 'LocalTunnel did not return a tunnel instance' });
            process.exit(1);
            return;
        }

        log({ event: 'ready', url: tunnel.url });

        // Wire events
        tunnel.on('close', () => log({ event: 'closed' }));
        tunnel.on('error', (e) => log({ event: 'error', message: e && e.message ? e.message : String(e) }));

        // Graceful stop via stdin "stop\n"
        var buf = '';
        process.stdin.setEncoding('utf8');
        process.stdin.on('data', function (chunk) {
            buf += chunk;
            var i;
            while ((i = buf.indexOf('\n')) !== -1) {
                var line = buf.slice(0, i).trim();
                buf = buf.slice(i + 1);
                if (line === 'stop') {
                    try { tunnel.close(); } catch { }
                    process.exit(0);
                }
            }
        });
        process.stdin.on('end', function () {
            try { tunnel.close(); } catch { }
            process.exit(0);
        });
    });

    // Some versions return the tunnel synchronously; keep reference
    if (!tunnel && maybe) tunnel = maybe;

} catch (e) {
    log({ event: 'error', message: e && e.message ? e.message : String(e) });
    process.exit(1);
}
