# Reporting Spec

## Canonical Status Values

Use exactly one:

- `implemented`
- `implemented_not_validated`
- `validated_with_known_gaps`
- `blocked`

## Confidence Values

Use exactly one:

- `high`
- `medium`
- `low`

## Minimum Evidence Standard

For any non-blocked report, include:

- at least one validation command or a clear statement that validation was not run
- at least one concrete file path
- at least one known gap or an explicit statement that none were found

## Example Validation Results

Good:

```md
## Validation Results
- `cargo check`: passed
- `bun run smoke:phase1`: passed
- Runtime verification: not run
```

Bad:

```md
## Validation Results
- Everything looks good
```

## Runtime Risks Prompts

Use this section to answer questions like:

- What could still fail in a real browser or app runtime?
- Which flows were only compile-validated?
- Which platform paths were not exercised?
- Which assumptions depend on external configuration or server support?

