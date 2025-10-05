#!/usr/bin/env node
// Minimal LocalTunnel controller without yargs.
// Usage: node lt-controller.js --port 7777 [--subdomain runic-123] [--host https://...]
const lt = require('localtunnel');

function log(obj) {
    try {
        process.stdout.write(JSON.stringify(obj) + '\n');
    } catch (_) {
        // swallow
    }
}

// Tiny arg parse: supports --key value and --key=value
function parseArgs(argv) {
    const out = {};
    for (let i = 0; i < argv.length; ++i) {
        const a = argv[i];
        if (!a.startsWith('--')) continue;

        const eq = a.indexOf('=');
        let key, val;

        if (eq !== -1) {
            key = a.slice(2, eq);
            val = a.slice(eq + 1);
        } else {
            key = a.slice(2);
            // next token is the value if present and not another flag
            if (i + 1 < argv.length && !argv[i + 1].startsWith('--')) {
                val = argv[++i];
            } else {
                val = 'true'; // boolean-style switch
            }
        }
        out[key] = val;
    }
    return out;
}

(async () => {
    const args = parseArgs(process.argv.slice(2));

    // Validate port
    const port = parseInt(args.port, 10);
    if (!Number.isFinite(port) || port <= 0 || port > 65535) {
        log({ event: 'error', message: 'Missing or invalid --port (1..65535)' });
        process.exit(1);
        return;
    }

    // Optional params
    const subdomain = typeof args.subdomain === 'string' ? args.subdomain : undefined;
    const host = typeof args.host === 'string' ? args.host : undefined;

    try {
        const tunnel = await lt({ port, subdomain, host });

        // Announce ready
        log({ event: 'ready', url: tunnel.url });

        // Wire events
        tunnel.on('close', () => log({ event: 'closed' }));
        tunnel.on('error', (e) => log({ event: 'error', message: (e && e.message) ? e.message : String(e) }));

        // Simple stdin command loop ("stop\n" to close)
        let buf = '';
        process.stdin.setEncoding('utf8');
        process.stdin.on('data', chunk => {
            buf += chunk;
            for (; ;) {
                const i = buf.indexOf('\n');
                if (i === -1) break;
                const line = buf.slice(0, i).trim();
                buf = buf.slice(i + 1);

                if (line === 'stop') {
                    try { tunnel.close(); } catch { }
                    process.exit(0);
                }
            }
        });
        process.stdin.on('end', () => {
            try { tunnel.close(); } catch { }
            process.exit(0);
        });

    } catch (e) {
        log({ event: 'error', message: (e && e.message) ? e.message : String(e) });
        process.exit(1);
    }
})();


//#!/usr/bin/env node
//const lt = require('localtunnel');

//function log(obj) {
//    process.stdout.write(JSON.stringify(obj) + "\n");
//}

//(async () => {
//    const argv = require('yargs/yargs')(process.argv.slice(2))
//        .option('port', { type: 'number', demandOption: true })
//        .option('subdomain', { type: 'string' })
//        .strict().argv;

//    try {
//        const tunnel = await lt({ port: argv.port, subdomain: argv.subdomain });
//        log({ event: 'ready', url: tunnel.url });

//        tunnel.on('close', () => log({ event: 'closed' }));
//        tunnel.on('error', (e) => log({ event: 'error', message: e?.message || String(e) }));

//        // accept "stop\n" on stdin for graceful shutdown (optional)
//        let buf = '';
//        process.stdin.setEncoding('utf8');
//        process.stdin.on('data', chunk => {
//            buf += chunk;
//            let i;
//            while ((i = buf.indexOf('\n')) !== -1) {
//                const line = buf.slice(0, i).trim();
//                buf = buf.slice(i + 1);
//                if (line === 'stop') {
//                    tunnel.close();
//                    process.exit(0);
//                }
//            }
//        });
//        process.stdin.on('end', () => { tunnel.close(); process.exit(0); });

//    } catch (e) {
//        log({ event: 'error', message: e?.message || String(e) });
//        process.exit(1);
//    }
//})();
