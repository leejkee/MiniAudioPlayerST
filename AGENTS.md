## Reference
- dev: @docs/Dev.md
- PRD: @docs/PRD.md

## Output
- Use **Chinese** for explanation
- Use **English** for git commit
- Do **not** use **emoji**

## Git Commit
- Follow the Conventional Commits format: `<type>: <description>`.
- Use a lowercase type such as `feat`, `fix`, `docs`, `refactor`, `test`, or `chore`.
- Write the description in concise English using the imperative mood, without a trailing period.
- Example: `feat: add test mode switch`

## Temporary Files
- Store all temporary files under the project-root `@tmp/` directory.
- Do not use system temporary directories for project tasks.

## Core Source Files
- Do not modify any source file under `firmware/MiniAudioPlayerST/Core/` except `firmware/MiniAudioPlayerST/Core/Src/main.c`.
