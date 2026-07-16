# Codex review workflow

Status: Active

Use this workflow for a GitHub issue branch or pull request that needs technical
review.

## Inputs

- canonical GitHub issue and acceptance criteria;
- pull request/branch diff and review threads;
- relevant architecture, design, and system docs;
- source, tests, CMake target definitions, and validation output;
- historical report only when it contains necessary evidence.

## Review questions

1. Is the change inside issue scope and the owning product boundary?
2. Does it introduce a reverse dependency or private include escape?
3. Does the implementation solve the cause rather than the final symptom?
4. Are error identity, recovery, logging, and sensitive context handled safely?
5. Do tests prove the changed boundary and original failure?
6. Were the claimed commands actually run?
7. Is runtime or out-of-tree package verification still required?

## Outcomes

- **approve** — acceptance criteria and evidence are complete;
- **revise** — concrete implementation or test changes are required;
- **verify** — implementation may be correct but required evidence is missing;
- **blocked** — an external dependency or required user decision prevents
  progress.

Record the outcome in the GitHub review/issue. Do not mirror it into a local
queue state.
