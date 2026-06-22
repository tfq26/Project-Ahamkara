# Master Log Spec

The master log is not a replacement for the full report.

Its job is to let another agent quickly answer:

- what changed
- what status the run ended in
- where the full report lives
- what the highest remaining risk is
- what should happen next

## Entry Rules

- Append, do not rewrite old entries unless correcting a factual mistake.
- Use reverse chronological order only if the file is already maintained that way. Otherwise append at the end.
- Keep each entry under 8 lines after the heading.
- Always include a `Report:` line that points to the full report path.

