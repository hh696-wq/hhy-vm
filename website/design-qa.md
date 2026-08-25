# Design QA

final result: passed

- Source: `../assets/website/ChatGPT Image 2026年8月25日 14_17_10.png`
- Implementation: `http://127.0.0.1:8100/zh`
- Comparison viewport: 941 × 900
- Responsive viewport: 390 × 844
- Comparison artifact: `qa-comparison-first-view.png` (local, gitignored)

## Visual comparison

The implementation preserves the reference hierarchy, white and light-blue palette,
two-column hero, branded HHY artwork, bordered code sample, feature cards, learning
path, release status, and compact language-project footer. Chinese copy and the
real V1.0.0 examples replace the English concept placeholders. Header and footer
navigation were intentionally simplified to remove overlapping destinations.

## Functional verification

- Chinese and English home pages render.
- English “Get Started” opens `/en/learn/quick-start`.
- All ten Learn chapters are statically generated in both languages.
- Mobile navigation opens at 390 px with no horizontal overflow.
- Footer domain, email, repository, specification, and license links resolve to
  their intended targets.
- Browser console: no errors or warnings.
- Eleven documentation snippets pass the real `hhy check` command.
- ESLint, TypeScript, and the optimized Next.js production build pass.

## Comparison history

1. Initial pass matched the reference sections but had redundant header and footer
   destinations and excessive tablet-height wrapping.
2. Final pass reduced the header to Learn, Spec, GitHub, and the language switch;
   changed Download from the repository homepage to the verified latest Release;
   grouped the footer as Learn, Project, and Contact; retained the desktop
   philosophy grid until 820 px; and verified the 8100-port implementation.
