## 🤖 Built with AI

Komai is a largely [AI-vibe-engineered](https://en.wikipedia.org/wiki/Artificial_intelligence) [Matrix](https://matrix.org/) chat application.

[AI-assisted software development](https://en.wikipedia.org/wiki/AI-assisted_software_development) is becoming more popular, and Komai leans into it fully.

Komai is built by a [team](https://etke.cc/about/) of professional software engineers using AI assistance to code in a language stack ([C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML)) which is not their strong suit.

We think that **AI in capable hands can deliver above-average results**. Could this work? Let's see!


### How AI is used

AI coding agents (primarily [Claude Code](https://code.claude.com/docs/en/overview) and [Codex](https://openai.com/index/introducing-codex/)) are used throughout most of Komai's development lifecycle:

- **Feature implementation** -- new features and UI components are typically implemented with AI assistance, often from start to finish
- **Bug fixing** -- diagnosing and fixing bugs across the C++/QML/Rust codebase
- **Refactoring** -- restructuring code, splitting modules, improving architecture
- **Code review** -- AI-assisted review of changes before they are committed
- **Documentation** -- most documentation (including this page) is written with AI assistance
- **Translations** -- AI-assisted gap filling for 30+ languages (see [Translations](../maintainers/translations.md))

Human engineers provide direction, review, testing, and final judgment. AI provides implementation velocity in an unfamiliar language stack.


### Can I also use AI to contribute?

Anyone can use AI and send [Pull Requests](https://github.com/etkecc/komai/pulls) for new features and bugfixes.

However, large changes require engineering judgment -- every change comes with tradeoffs, and AI alone won't navigate them well. If you're not comfortable reviewing and defending the code your AI produces, please submit an [Issue](https://github.com/etkecc/komai/issues) instead. It's more helpful than a large PR the team has to untangle.


### The name connection

The name **Komai** can also be read as **comm**unications + **AI** -- a nod to the role AI plays in building this application. For the full naming story, see [🦁 Identity](identity.md).
