# 13. Project: Asset Governance

Audit project assets, generate a governance report, and safely execute copy, move, and remove remediations with Runtime-native dry-run.

## 13.1 Separate audit from remediation

Asset Governance consists of audit.hhy and cleanup.hhy. The auditor scans source, configuration, images, video, and build outputs for oversized, stale, badly named, duplicate-text, and possibly sensitive files. The cleaner accepts only allow-listed report actions and never assembles shell commands.


| Check or action | HHY implementation |
| --- | --- |
| Inventory and size | files, File.size, and Bytes |
| Stale files | File.modified, now, and Duration |
| Naming and secrets | Regex, read_text, and redacted findings |
| Duplicate content | group_by text content without storing source text in the report |
| Remediation | copy, move, remove, and --dry-run EffectDispatcher |


[View the complete Asset Governance source on GitHub ↗](https://github.com/hh696-wq/hhy-vm/tree/main/practical-projects/asset-governance)

Includes auditor, cleaner, four HHY modules, risky fixtures, and dry-run plus applied-remediation assertions.


## 13.2 Project layout

![Asset Governance project tree](https://hhylang.dev/asset-governance-tree.png)

_The real layout contains audit and cleanup entry points, governance modules, and intentionally large, stale, duplicate, and sensitive fixtures._


| Program | Responsibility |
| --- | --- |
| audit.hhy | Scan and atomically write report.json; return 1 when a critical finding exists |
| cleanup.hhy | Read report.actions and execute controlled copy/move/remove operations |
| self-test.sh | Create an isolated mktemp workspace, dry-run first, then apply and assert every action |


## 13.3 Actual self-test and dry-run

```sh
cd hhy-vm
sh practical-projects/asset-governance/self-test.sh
```


![Actual Asset Governance audit, dry-run, and applied-remediation terminal output](https://hhylang.dev/asset-governance-self-test.png)

_Actual run: detects large/naming/stale/sensitive/duplicate findings; dry-run prints the effect plan without changes; all assertions pass after three real actions._


{% hint style="info" %}
Processed means that program control reached the action; Runtime intercepts its effect during dry-run. The test then proves that the workspace is unchanged, and verifies copy, move, and remove only after the real run.
{% endhint %}


## 13.4 Two-phase operation

Run the audit and inspect report.json first. audit returns 1 for a critical finding but still writes the complete report. After approving actions, run dry-run, inspect Runtime's effect plan, and only then apply remediation.


```sh
hhy run practical-projects/asset-governance/audit.hhy ./project ./config.json ./report.json
hhy run --dry-run practical-projects/asset-governance/cleanup.hhy ./project ./report.json
hhy run practical-projects/asset-governance/cleanup.hhy ./project ./report.json
```
