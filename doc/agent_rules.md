model = "gpt-5.4"

[project]
instructions = """
Follow docs/product_spec.md, docs/architecture.md, docs/agent_playbook.md, docs/product_plan.md

Prioritize:
1. correctness of pixel inspection
2. minimal dependencies
3. cross-platform code
4. small, reviewable changes

Do not:
- add features outside the documented roadmap
- introduce large frameworks without justification
- hide RAW interpretation assumptions
- break the separation between source data, interpretation state, and view state

Always:
- update docs when architecture changes
- add tests for pixel correctness and histogram correctness when applicable
- keep files small and responsibilities narrow
"""