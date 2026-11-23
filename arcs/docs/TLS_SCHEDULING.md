# ARCS TLS scheduling

The shared Link session must regain control after a nonblocking read so it can
drain outbound requests. A zero-timeout `plat_tls_read` now returns
`PLAT_TLS_TIMEOUT` on WANT_READ/WANT_WRITE. Positive timeouts share one elapsed
time budget across TLS retries instead of restarting the full wait each time.

ARCS's SAL socket also requests TCP_NODELAY after connect. Failure is a warning,
not a connection failure. Completed TLS writes taking at least 250 ms report
only byte count and duration. Certificate verification and the existing 15 s
write failure boundary remain in use.

Run `ARCS_BASE=/path/to/arcs-sdk-0.1.6 sh arcs/tests/host/run_transport.sh` from
the repository root. The harness executes the actual platform implementation
with deterministic TLS outcomes and time, covering WANT_READ/WANT_WRITE,
total timeout, data, peer close, errors, and TCP_NODELAY arguments/failure.

On the tested ARCS Mini device, small TLS writes still sometimes stalled for
15 s and triggered reconnects after these changes. These fixes bound scheduling
and provide diagnostics; they do not establish a stable network connection.
