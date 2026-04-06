## 🦁 Identity

<p align="center">
    <img src="../../resources/komai.svg" alt="Komai logo" width="128" />
</p>
<h1 align="center">Komai (<a target="_blank" href="https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84">こまい</a>)</h1>
<h2 align="center">A fine <a target="_blank" href="https://matrix.org/">Matrix</a> chat app you can get to love</h2>

Komai started as a [fork](https://en.wikipedia.org/wiki/Fork_(software_development)) of the [nheko](https://nheko.im/nheko-reborn/nheko) Matrix chat application, but has since been heavily rebuilt into its own client.

This page covers both Komai's direction as a project and the story behind its name.

### 🛣️ From fork to independent direction

Komai began life as a fork of [nheko](https://nheko.im/nheko-reborn/nheko). At first, we used original upstream sources and applied small UX-focused patches on top.

After roughly **100** patches, this approach became hard to sustain. Tracking upstream while carrying many local changes created significant maintenance cost for limited benefit.

We also recognized that many of these changes reflected Komai-specific product decisions and were unlikely to be fully upstreamed. Rather than spending a long time reconciling divergent goals, we chose to focus on quickly shipping a polished client aligned with Komai's vision.

Over time, that also meant replacing major parts of the original technical stack. The old `mtxclient` + `libolm` Matrix core was removed in favor of the Rust [matrix-sdk](https://github.com/matrix-org/matrix-rust-sdk), and more application logic kept moving from C++ into Rust as Komai evolved.

At that point, Komai stopped being "nheko with patches" and became a different application with clear upstream ancestry.

### 🏷️ Name

Komai's name is very much inspired by [nheko](https://nheko.im/nheko-reborn/nheko). In Japanese, *neko* ([猫](https://en.wiktionary.org/wiki/%E7%8C%AB)) means cat 🐱.

Komai started out as a [🐱 little cat](#little-cat) which met a [🦁🐶 lion-dog](#cat-meets-dog) guardian of a [Shinto shrine](https://en.wikipedia.org/wiki/Shinto_shrine) ⛩️, but that's [🤌 a small thing](#it-s-a-small-thing) in the grand scheme of things.

After all, it's [🤖 all about AI](#it-s-an-ai-thing) nowadays and we all know that [Matrix](https://matrix.org/) needs at least [❤️ a little love](#it-s-a-love-thing).


#### 🐱 Little cat

We thought of naming this application koneko ([子猫](https://en.wiktionary.org/wiki/%E5%AD%90%E7%8C%AB) = kitten = small cat), but decided that this is too close to the original name.


#### 🐶 Cat meets dog

Inspired by [little cat](#little-cat), we tried to come up with an abbreviation for a made-up word meaning "small (*ko*) Matrix (*ma*) dog (*inu*)" = Komainu.

It turns out that [Komainu](https://en.wikipedia.org/wiki/Komainu) (狛犬) are the mythical **lion-dog** (🦁🐶) guardians of [Shinto shrines](https://en.wikipedia.org/wiki/Shinto_shrine) ⛩️.

Because Komainu is a tad too long of a word, we settled on a shortened version: **Komai**.

If *nheko* nods to *neko* (猫, "cat"), then *Komai* answers with the *lion-dog*, a cat and a dog at the same time.

A fork that is both a quiet kinship and a playful contrast with its upstream.


#### 🤌 It's a small thing

Despite originally being shortened from the [Komainu](https://en.wikipedia.org/wiki/Komainu) (狛犬) lion-dog guardian, *Komai* ([こまい](https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84)) also happens to be another Japanese word.

It means **fine/slender**, though spellings and kanji use vary by region.


#### 🤖 It's an AI thing

You can also think of **Komai** as **comm**unications + **AI**, because it's a largely [AI-vibe-engineered](https://en.wikipedia.org/wiki/Artificial_intelligence) Matrix chat application.

[AI-assisted software development](https://en.wikipedia.org/wiki/AI-assisted_software_development) is becoming more popular.

Komai is a [proof-of-concept](https://en.wikipedia.org/wiki/Proof_of_concept) application, built by a team of professional software engineers using AI assistance to code in a language stack ([C++](https://en.wikipedia.org/wiki/C%2B%2B) and [QML](https://en.wikipedia.org/wiki/QML)) which is not their strong suit.

We think that **AI in capable hands can deliver above-average results**. Could this work? Let's see!


#### ❤️ It's a love thing

You can also think of **Komai** as **ko** (small = [小](https://en.wiktionary.org/wiki/%E5%B0%8F)) + **m** (Matrix) + **ai** (love = [愛](https://en.wiktionary.org/wiki/%E6%84%9B)).

We'd like **Komai** to be a fine (small & refined) Matrix chat app you can get to love.
