ARTIFACT — stability_promise.md

# Stability Promise & Release Channels (Product Strategy)

This project is intended to become a durable product with predictable behavior.

## 1) Two Release Channels (Recommended)

### Development (Open)
- Open-source, fast iteration
- New features and experiments
- Docs may lag behavior (allowed, but tracked)

### Stable (Product)
- Slower changes, heavy bench testing
- Backwards-compat promise within v1.x
- Intended for real installations you depend on

## 2) What “Stable” Means (Contract)

A stable release must ship with:
- Updated canonical docs
- Bench Test Checklist results (date + tester + notes)
- Changelog entries
- A migration note if config/log schema changes

## 3) Passive Income Alignment (Non-invasive)

“Stable channel” value can be:
- tested firmware binaries
- printed wiring diagrams + assembly guide
- curated kit BOM and known-good parts list
- priority support / install service

None of the above requires cloud dependence.
