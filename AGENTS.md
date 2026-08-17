# Fast Viewer — Agent Operating Rules

These rules are mandatory for all future coding agents working in this repository.

## Mandatory reading order (before modifying code)

1. Read `PROJECT.md`
2. Read `UX.md`
3. Read `ARCHITECTURE.md`
4. Read `PERFORMANCE.md`
5. Read `TASK.md`
6. Inspect existing repository state
7. Make the smallest coherent plan

## Execution discipline

- Preserve scope.
- Prefer small changes.
- Compile frequently.
- Test actual behavior.
- Measure performance rather than claiming it.
- Never report a test as passing unless it was executed successfully.
- Never suppress failures merely to progress.
- Never weaken a product requirement because it is difficult to test.
- Never add unrelated features.
- Never install a heavy framework without explicit human approval.

## Scope enforcement

- Fast Viewer displays images only. Windows Explorer owns files.
- The prohibited-features list in `PROJECT.md` is binding. Do not add features from it — or any speculative feature — without explicit human approval.
- No speculative "future-proof" architecture.
- No invented milestones beyond M0 / M1 / M2 without human approval.

## When to stop and ask

- If a specification is ambiguous, do not invent product behavior. Stop and ask the human owner when the choice materially affects UX, architecture, dependency size, or performance.
- If implementation becomes substantially more complex than expected: stop, explain why, propose the smallest alternatives, and wait for approval.

## Testing rules

- Only report tests that actually ran and passed.
- If a requirement is hard to test, still meet it — do not weaken it.
- Benchmark before claiming performance improvements. M0 must be benchmarked before M1 begins.
