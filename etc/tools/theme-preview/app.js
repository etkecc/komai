const PALETTE_KEYS = [
  "window",
  "windowText",
  "base",
  "alternateBase",
  "text",
  "brightText",
  "button",
  "buttonText",
  "light",
  "mid",
  "dark",
  "highlight",
  "highlightedText",
  "link",
  "toolTipBase",
  "toolTipText",
  "attention",
  "success",
  "warning",
  "error",
];

const COMMUNITY_ITEMS = [
  { icon: "AR", label: "All rooms" },
  { icon: "FV", label: "Favourites" },
  { icon: "PE", label: "People" },
  { icon: "BT", label: "Bots" },
  { icon: "GR", label: "Groups", hover: true },
  { icon: "LP", label: "Low priority" },
  { icon: "PS", label: "Product Space", selected: true },
  { icon: "EN", label: "Engineering" },
  { icon: "MK", label: "Marketing" },
];

const ROOM_ITEMS = [
  {
    avatar: "GE",
    title: "#general",
    preview: "You: Theme preview should load YAML files directly now.",
    time: "16:20",
    selected: true,
  },
  {
    avatar: "DX",
    title: "#design-exploration",
    preview: "Hover demo: room header and message layout look closer now.",
    time: "15:08",
    hover: true,
  },
  {
    avatar: "OP",
    title: "#ops",
    preview: "The release note draft is waiting for a final pass.",
    time: "14:11",
  },
  {
    avatar: "QA",
    title: "#qa",
    preview: "Link checks and interaction testing passed on the new preview page.",
    time: "12:47",
  },
  {
    avatar: "DS",
    title: "#design-system",
    preview: "Reply styling, hover states, and selected rows all need to stay visible.",
    time: "11:03",
  },
  {
    avatar: "AN",
    title: "#announcements",
    preview: "Tomorrow: theme guide cleanup and broader palette review.",
    time: "Yesterday",
  },
];

const OTHER_SENDERS = [
  "Aalto",
  "Bora",
  "Cyra",
  "Deni",
  "Edda",
  "Faris",
  "Gita",
  "Hiro",
  "Inga",
  "Juno",
  "Kian",
  "Lumi",
  "Mara",
  "Niko",
];

const OTHER_MESSAGE_TEMPLATES = [
  "Morning. The live YAML loader is much better than regenerating a JSON payload for every tweak.",
  "The room-list action bar now looks closer to the real app instead of a generic dashboard card.",
  "I kept the frame flat so selected orange rows and subtle separators do most of the work.",
  "The theme guide should mention this preview page because it is now the fastest way to compare built-ins.",
  "This message includes a link to <a href=\"https://github.com/etkecc/komai/issues/3\">issue #3</a> so link contrast stays visible.",
  "The uploaded-theme flow is useful because it lets us try custom files without putting them in the repo.",
  "One thing I still want to compare later is whether the room-header controls need slightly more spacing.",
  "The hover state stays explicit here, but it does not overwhelm the selected room.",
  "For light themes, the biggest wins are still readable orange selections and calmer bubble fills.",
  "This should make broader theme cleanup much faster because the whole gallery is one long scroll.",
  "The preview page should stay honest to actual palette roles: sidebars from alternateBase, rows from window, timeline from base.",
  "I also wanted the space-related action bar visible, because it changes the room-list silhouette in a real session.",
  "Please verify the uploaded-theme scroll behavior too, especially if the file slug matches an existing built-in.",
  "Another link for checking: <a href=\"https://github.com/etkecc/komai/issues/7\">issue #7</a> still matters for modal contrast.",
];

const SELF_MESSAGE_TEMPLATES = [
  "Ugh. Why are generated preview pages always a little too magical until we strip them back down?",
  "The next pass can focus on finer layout polish, but the workflow should be right now: one page, live themes, upload custom YAML, keep scrolling.",
];

const REQUIRED_FIELDS = ["name", "variant", "palette", "userColors"];
const THEME_CACHE_BUSTER = `${Date.now()}`;
const state = {
  themes: [],
  dragDepth: 0,
};

const galleryEl = document.getElementById("theme-gallery");
const indexEl = document.getElementById("theme-index");
const uploadButtonEl = document.getElementById("upload-button");
const uploadInputEl = document.getElementById("upload-input");
const statusMessageEl = document.getElementById("status-message");
const dropOverlayEl = document.getElementById("drop-overlay");

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll("\"", "&quot;")
    .replaceAll("'", "&#39;");
}

function slugify(value) {
  return String(value)
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "") || "theme";
}

function uniqueSlug(preferredSlug, existingSlugs) {
  let slug = preferredSlug;
  let index = 2;
  while (existingSlugs.has(slug)) {
    slug = `${preferredSlug}-${index}`;
    index += 1;
  }
  return slug;
}

function parseColor(hex) {
  const value = String(hex).trim().replace(/^#/, "");
  if (!/^[0-9a-f]{6}$/i.test(value)) {
    throw new Error(`Invalid hex color: ${hex}`);
  }
  return [
    Number.parseInt(value.slice(0, 2), 16),
    Number.parseInt(value.slice(2, 4), 16),
    Number.parseInt(value.slice(4, 6), 16),
  ];
}

function rgbToHex(rgb) {
  return `#${rgb.map((component) => component.toString(16).padStart(2, "0")).join("")}`;
}

function linearize(component) {
  const value = component / 255;
  return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4;
}

function luminance(hex) {
  const [r, g, b] = parseColor(hex);
  return 0.2126 * linearize(r) + 0.7152 * linearize(g) + 0.0722 * linearize(b);
}

function contrastRatio(left, right) {
  const l1 = luminance(left);
  const l2 = luminance(right);
  const lighter = Math.max(l1, l2);
  const darker = Math.min(l1, l2);
  return (lighter + 0.05) / (darker + 0.05);
}

function rgbToHsv(hex) {
  const [r, g, b] = parseColor(hex).map((component) => component / 255);
  const max = Math.max(r, g, b);
  const min = Math.min(r, g, b);
  const delta = max - min;
  let hue = 0;

  if (delta !== 0) {
    switch (max) {
      case r:
        hue = ((g - b) / delta) % 6;
        break;
      case g:
        hue = (b - r) / delta + 2;
        break;
      default:
        hue = (r - g) / delta + 4;
        break;
    }
    hue /= 6;
    if (hue < 0) {
      hue += 1;
    }
  }

  const saturation = max === 0 ? 0 : delta / max;
  return [hue, saturation, max];
}

function hsvToRgb(h, s, v) {
  const scaledHue = (h % 1) * 6;
  const sector = Math.floor(scaledHue);
  const fraction = scaledHue - sector;
  const p = v * (1 - s);
  const q = v * (1 - fraction * s);
  const t = v * (1 - (1 - fraction) * s);
  let r = 0;
  let g = 0;
  let b = 0;

  switch (sector) {
    case 0:
      [r, g, b] = [v, t, p];
      break;
    case 1:
      [r, g, b] = [q, v, p];
      break;
    case 2:
      [r, g, b] = [p, v, t];
      break;
    case 3:
      [r, g, b] = [p, q, v];
      break;
    case 4:
      [r, g, b] = [t, p, v];
      break;
    default:
      [r, g, b] = [v, p, q];
      break;
  }

  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

function qcolorDarker(hex, factor) {
  const [h, s, v] = rgbToHsv(hex);
  const nextValue = Math.max(0, Math.min(1, (v * 100) / factor));
  return rgbToHex(hsvToRgb(h, s, nextValue));
}

function qcolorLighter(hex, factor) {
  const [h, s, v] = rgbToHsv(hex);
  const nextValue = Math.max(0, Math.min(1, (v * factor) / 100));
  return rgbToHex(hsvToRgb(h, s, nextValue));
}

function deriveReadableAccentTextColor(accentColor, backgroundColor, minContrast = 4.5) {
  if (contrastRatio(accentColor, backgroundColor) >= minContrast) {
    return accentColor;
  }

  const preferDarker = contrastRatio("#000000", backgroundColor) >= contrastRatio("#ffffff", backgroundColor);
  let bestColor = null;
  let bestDistance = Number.POSITIVE_INFINITY;
  let bestContrast = 0;

  function consider(candidate, distance) {
    const ratio = contrastRatio(candidate, backgroundColor);
    if (ratio < minContrast) {
      return;
    }
    if (
      bestColor === null
      || distance < bestDistance
      || (distance === bestDistance && ratio > bestContrast)
    ) {
      bestColor = candidate;
      bestDistance = distance;
      bestContrast = ratio;
    }
  }

  for (let factor = 105; factor <= 400; factor += 5) {
    if (preferDarker) {
      consider(qcolorDarker(accentColor, factor), factor - 100);
      consider(qcolorLighter(accentColor, factor), factor - 100);
    } else {
      consider(qcolorLighter(accentColor, factor), factor - 100);
      consider(qcolorDarker(accentColor, factor), factor - 100);
    }

    if (bestColor !== null && bestDistance === 5) {
      break;
    }
  }

  if (bestColor !== null) {
    return bestColor;
  }

  return contrastRatio("#000000", backgroundColor) >= contrastRatio("#ffffff", backgroundColor)
    ? "#000000"
    : "#ffffff";
}

function parseYaml(text) {
  const result = {};
  let currentSection = null;
  let currentSubsection = null;

  for (const rawLine of text.split(/\r?\n/u)) {
    const line = rawLine.replace(/\s+$/u, "");
    if (!line || /^\s*#/u.test(line)) {
      continue;
    }

    let match;
    if (line.startsWith("    - ")) {
      if (currentSection && currentSubsection) {
        match = line.match(/^    - "([^"]*)"$/u) || line.match(/^    - ([^"#\s]+)\s*(?:#.*)?$/u);
        if (match) {
          const value = match[1].trim();
          if (value) {
            result[currentSection][currentSubsection].push(value);
          }
        }
      }
      continue;
    }

    if (line.startsWith("  ")) {
      currentSubsection = null;
      if (!currentSection) {
        continue;
      }
      match = line.match(/^  (\w+):\s*"([^"]*)"$/u) || line.match(/^  (\w+):\s*([^"#\s]*)\s*(?:#.*)?$/u);
      if (match) {
        const [, key, rawValue] = match;
        const value = rawValue.trim();
        if (value) {
          result[currentSection][key] = value;
        } else {
          result[currentSection][key] = [];
          currentSubsection = key;
        }
      }
      continue;
    }

    match = line.match(/^(\w+):\s*"([^"]*)"$/u) || line.match(/^(\w+):\s*([^"#\s]*)\s*(?:#.*)?$/u);
    if (match) {
      const [, key, rawValue] = match;
      const value = rawValue.trim();
      currentSubsection = null;
      if (value) {
        result[key] = value;
        currentSection = null;
      } else {
        result[key] = {};
        currentSection = key;
      }
    }
  }

  return result;
}

function validateTheme(rawTheme, sourceName) {
  for (const field of REQUIRED_FIELDS) {
    if (!(field in rawTheme)) {
      throw new Error(`${sourceName}: missing required field '${field}'`);
    }
  }

  if (!["light", "dark"].includes(rawTheme.variant)) {
    throw new Error(`${sourceName}: variant must be 'light' or 'dark'`);
  }

  if (typeof rawTheme.palette !== "object" || rawTheme.palette === null) {
    throw new Error(`${sourceName}: palette must be a mapping`);
  }

  for (const key of PALETTE_KEYS) {
    if (!(key in rawTheme.palette)) {
      throw new Error(`${sourceName}: missing palette.${key}`);
    }
    parseColor(rawTheme.palette[key]);
  }

  if (typeof rawTheme.userColors !== "object" || rawTheme.userColors === null) {
    throw new Error(`${sourceName}: userColors must be a mapping`);
  }

  if (!("self" in rawTheme.userColors)) {
    throw new Error(`${sourceName}: missing userColors.self`);
  }
  parseColor(rawTheme.userColors.self);

  if (!Array.isArray(rawTheme.userColors.others) || rawTheme.userColors.others.length < 1) {
    throw new Error(`${sourceName}: userColors.others must contain at least one color`);
  }

  rawTheme.userColors.others.forEach((color, index) => {
    try {
      parseColor(color);
    } catch (error) {
      throw new Error(`${sourceName}: invalid userColors.others[${index}]`);
    }
  });
}

function buildThemeModel(rawTheme, options) {
  const sourceName = options.sourceName || "theme";
  validateTheme(rawTheme, sourceName);

  const existingSlugs = new Set(state.themes.map((theme) => theme.slug));
  const baseSlug = options.slug
    ? slugify(options.slug)
    : slugify(`${rawTheme.name}-${rawTheme.variant}`);
  const slug = uniqueSlug(baseSlug, existingSlugs);
  const palette = { ...rawTheme.palette };
  const userColors = {
    self: rawTheme.userColors.self,
    others: [...rawTheme.userColors.others],
  };

  return {
    slug,
    name: rawTheme.name,
    author: rawTheme.author || "",
    variant: rawTheme.variant,
    palette,
    userColors,
    sourceType: options.sourceType || "builtin",
    sourceName,
    preview: {
      senderTextOnBase: {
        self: deriveReadableAccentTextColor(userColors.self, palette.base),
        others: userColors.others.map((color) => deriveReadableAccentTextColor(color, palette.base)),
      },
      ratios: {
        windowText: contrastRatio(palette.window, palette.text).toFixed(2),
        alternateBaseText: contrastRatio(palette.alternateBase, palette.text).toFixed(2),
        highlightHighlightedText: contrastRatio(palette.highlight, palette.highlightedText).toFixed(2),
        windowAlternateBase: contrastRatio(palette.window, palette.alternateBase).toFixed(2),
      },
    },
  };
}

function sortThemes(themes) {
  return [...themes].sort((left, right) => {
    if (left.variant !== right.variant) {
      return left.variant === "light" ? -1 : 1;
    }
    return left.slug.localeCompare(right.slug);
  });
}

function themeUrl(path) {
  const url = new URL(path, window.location.href);
  url.searchParams.set("v", THEME_CACHE_BUSTER);
  return url.toString();
}

async function loadBuiltinThemes() {
  const listingResponse = await fetch(themeUrl("resources/themes/"), { cache: "no-store" });
  if (!listingResponse.ok) {
    throw new Error(`Failed to list themes: HTTP ${listingResponse.status}`);
  }

  const entries = await listingResponse.json();
  const files = Array.isArray(entries)
    ? entries
        .filter((entry) => entry && entry.type === "file" && /\.ya?ml$/iu.test(entry.name))
        .map((entry) => entry.name)
    : [];

  if (files.length === 0) {
    throw new Error("No theme YAML files found under /resources/themes/");
  }

  const themes = await Promise.all(
    files.map(async (fileName) => {
      const response = await fetch(
        themeUrl(`resources/themes/${encodeURIComponent(fileName)}`),
        { cache: "no-store" },
      );
      if (!response.ok) {
        throw new Error(`Failed to load ${fileName}: HTTP ${response.status}`);
      }

      const text = await response.text();
      const rawTheme = parseYaml(text);
      return buildThemeModel(rawTheme, {
        slug: fileName.replace(/\.[^.]+$/u, ""),
        sourceName: fileName,
        sourceType: "builtin",
      });
    }),
  );

  return sortThemes(themes);
}

function setStatus(message, tone = "info") {
  statusMessageEl.textContent = message;
  statusMessageEl.dataset.tone = tone;
}

function applyThemeVars(node, theme) {
  const vars = {
    "--window": theme.palette.window,
    "--base": theme.palette.base,
    "--alternate-base": theme.palette.alternateBase,
    "--text": theme.palette.text,
    "--button-text": theme.palette.buttonText,
    "--bright-text": theme.palette.brightText,
    "--highlight": theme.palette.highlight,
    "--highlighted-text": theme.palette.highlightedText,
    "--link": theme.palette.link,
    "--mid": theme.palette.mid,
    "--dark": theme.palette.dark,
    "--attention": theme.palette.attention,
  };

  Object.entries(vars).forEach(([name, value]) => {
    node.style.setProperty(name, value);
  });
}

function ratioChip(label, value) {
  return `
    <div class="theme-meta__ratio">
      <span>${escapeHtml(label)}</span>
      <strong>${escapeHtml(value)}</strong>
    </div>
  `;
}

function renderCommunityItem(item) {
  const stateClass = item.selected
    ? "community-item is-selected"
    : item.hover
      ? "community-item is-hover-demo"
      : "community-item";
  return `
    <div class="${stateClass}">
      <div class="community-item__icon">${escapeHtml(item.icon)}</div>
      <div class="community-item__label">${escapeHtml(item.label)}</div>
    </div>
  `;
}

function renderRoomItem(item) {
  const stateClass = item.selected
    ? "room-item is-selected"
    : item.hover
      ? "room-item is-hover-demo"
      : "room-item";
  return `
    <div class="${stateClass}">
      <div class="room-item__avatar">${escapeHtml(item.avatar)}</div>
      <div class="room-item__body">
        <div class="room-item__topline">
          <strong>${escapeHtml(item.title)}</strong>
          <span>${escapeHtml(item.time)}</span>
        </div>
        <div class="room-item__preview">${escapeHtml(item.preview)}</div>
      </div>
    </div>
  `;
}

function buildMessages(theme) {
  const messages = [];
  const others = theme.userColors.others;
  const senderText = theme.preview.senderTextOnBase.others;

  others.forEach((bubbleColor, index) => {
    if (index === 3) {
      messages.push({
        type: "self",
        sender: "You",
        bubbleColor: theme.userColors.self,
        senderColor: theme.preview.senderTextOnBase.self,
        time: "16:03",
        text: SELF_MESSAGE_TEMPLATES[0],
      });
    }

    messages.push({
      type: "other",
      sender: OTHER_SENDERS[index % OTHER_SENDERS.length],
      bubbleColor,
      senderColor: senderText[index % senderText.length],
      time: `16:${String(4 + index).padStart(2, "0")}`,
      text: OTHER_MESSAGE_TEMPLATES[index % OTHER_MESSAGE_TEMPLATES.length],
      reply: index === 0
        ? {
            sender: "Morning",
            preview: "This reply block helps check inline surfaces too.",
          }
        : null,
    });
  });

  messages.push({
    type: "self",
    sender: "You",
    bubbleColor: theme.userColors.self,
    senderColor: theme.preview.senderTextOnBase.self,
    time: "16:20",
    text: SELF_MESSAGE_TEMPLATES[1],
  });

  return messages;
}

function renderMessage(message) {
  const sideClass = message.type === "self" ? "message message--self" : "message";
  const indicator = message.type === "self" ? "o oo" : "o";
  const replyHtml = message.reply
    ? `
      <div class="bubble__reply">
        <strong>${escapeHtml(message.reply.sender)}</strong>
        <span>${escapeHtml(message.reply.preview)}</span>
      </div>
    `
    : "";

  return `
    <article class="${sideClass}">
      <div class="message__sender" style="color: ${escapeHtml(message.senderColor)}">${escapeHtml(message.sender)}</div>
      <div class="bubble" style="--bubble-color: ${escapeHtml(message.bubbleColor)}">
        ${replyHtml}
        <p>${message.text}</p>
      </div>
      <div class="message__meta">${escapeHtml(indicator)} ${escapeHtml(message.time)}</div>
    </article>
  `;
}

function renderThemeSection(theme) {
  const section = document.createElement("section");
  section.className = "theme-section";
  section.id = theme.slug;
  applyThemeVars(section, theme);

  const sourceLabel = theme.sourceType === "uploaded"
    ? `Uploaded · ${theme.sourceName}`
    : `${theme.name}${theme.author ? ` · ${theme.author}` : ""}`;

  section.innerHTML = `
    <div class="theme-heading">
      <div class="theme-heading__copy">
        <h2><a href="#${escapeHtml(theme.slug)}">${escapeHtml(theme.slug)}</a></h2>
        <div class="theme-heading__meta">
          <span class="theme-heading__pill">${escapeHtml(theme.variant)}</span>
          <span class="theme-heading__pill theme-heading__pill--source">${escapeHtml(theme.sourceType)}</span>
          <span>${escapeHtml(sourceLabel)}</span>
        </div>
      </div>
      <div class="theme-meta">
        ${ratioChip("window/text", theme.preview.ratios.windowText)}
        ${ratioChip("alternateBase/text", theme.preview.ratios.alternateBaseText)}
        ${ratioChip("highlight/highlightedText", theme.preview.ratios.highlightHighlightedText)}
        ${ratioChip("window/alternateBase", theme.preview.ratios.windowAlternateBase)}
      </div>
    </div>

    <div class="preview-window">
      <div class="preview-window__chrome">
        <div class="preview-window__appmark">K</div>
        <div class="preview-window__title">Komai</div>
        <div class="preview-window__controls">
          <span></span><span></span><span></span>
        </div>
      </div>

      <div class="preview-window__body">
        <aside class="communities-panel">
          ${COMMUNITY_ITEMS.map(renderCommunityItem).join("")}
        </aside>

        <section class="roomlist-panel">
          <div class="roomlist-toolbar">
            <div class="roomlist-toolbar__avatar">ME</div>
            <div class="roomlist-toolbar__actions">
              <button type="button" class="toolbar-action">+ New</button>
              <button type="button" class="toolbar-action">o Switch</button>
            </div>
          </div>

          <div class="space-bar">
            <div class="space-bar__avatar">PS</div>
            <div class="space-bar__name">Product Space</div>
            <button type="button" class="space-bar__leave">Leave</button>
          </div>

          <div class="roomlist-items">
            ${ROOM_ITEMS.map(renderRoomItem).join("")}
          </div>
        </section>

        <section class="timeline-panel">
          <header class="room-header">
            <div class="room-header__identity">
              <div class="room-header__avatar">GE</div>
              <div class="room-header__text">
                <strong>#general</strong>
                <span>Private</span>
              </div>
            </div>
            <div class="room-header__actions">
              <button type="button" class="room-header__button">Search</button>
              <button type="button" class="room-header__button">11 member(s)</button>
              <button type="button" class="room-header__button">Unencrypted</button>
              <button type="button" class="room-header__button">Leave</button>
            </div>
          </header>

          <div class="timeline-scroll">
            <div class="timeline-separator">3 hours later</div>
            <div class="timeline-messages">
              ${buildMessages(theme).map(renderMessage).join("")}
            </div>
          </div>

          <footer class="composer">
            <button type="button" class="composer__button">+</button>
            <div class="composer__field">Write a message...</div>
            <button type="button" class="composer__button">:)</button>
            <button type="button" class="composer__send">></button>
          </footer>
        </section>
      </div>
    </div>
  `;

  return section;
}

function renderIndex(themes) {
  const lightThemes = themes.filter((theme) => theme.variant === "light");
  const darkThemes = themes.filter((theme) => theme.variant === "dark");

  indexEl.innerHTML = `
    <div class="theme-index__groups">
      ${[
        ["light", lightThemes],
        ["dark", darkThemes],
      ]
        .map(
          ([label, groupThemes]) => `
            <div class="theme-index__group">
              <div class="theme-index__group-title">${escapeHtml(label)}</div>
              <div class="theme-index__links">
                ${groupThemes
                  .map(
                    (theme) => `
                      <a class="theme-index__link" href="#${escapeHtml(theme.slug)}">
                        <span>${escapeHtml(theme.slug)}</span>
                        <span class="theme-index__variant">${escapeHtml(theme.sourceType)}</span>
                      </a>
                    `,
                  )
                  .join("")}
              </div>
            </div>
          `,
        )
        .join("")}
    </div>
  `;
}

function renderGallery() {
  galleryEl.innerHTML = "";
  state.themes.forEach((theme) => {
    galleryEl.append(renderThemeSection(theme));
  });
  renderIndex(state.themes);
}

async function addUploadedFiles(fileList) {
  const files = [...fileList].filter((file) => /\.ya?ml$/iu.test(file.name));
  if (files.length === 0) {
    setStatus("No YAML files were detected in that drop.", "error");
    return;
  }

  let firstNewSlug = null;
  const addedSlugs = [];

  for (const file of files) {
    try {
      const text = await file.text();
      const rawTheme = parseYaml(text);
      const theme = buildThemeModel(rawTheme, {
        slug: file.name.replace(/\.[^.]+$/u, ""),
        sourceName: file.name,
        sourceType: "uploaded",
      });
      state.themes.push(theme);
      addedSlugs.push(theme.slug);
      if (firstNewSlug === null) {
        firstNewSlug = theme.slug;
      }
    } catch (error) {
      console.error(error);
      setStatus(`Failed to add ${file.name}: ${error.message}`, "error");
    }
  }

  state.themes = sortThemes(state.themes);
  renderGallery();

  if (addedSlugs.length > 0) {
    setStatus(`Added ${addedSlugs.length} uploaded theme${addedSlugs.length === 1 ? "" : "s"}.`, "success");
  }

  if (firstNewSlug !== null) {
    const section = document.getElementById(firstNewSlug);
    if (section) {
      section.scrollIntoView({ behavior: "smooth", block: "start" });
      history.replaceState(null, "", `#${firstNewSlug}`);
    }
  }
}

function hasFiles(event) {
  return Array.from(event.dataTransfer?.types || []).includes("Files");
}

function showDropOverlay() {
  dropOverlayEl.hidden = false;
}

function hideDropOverlay() {
  dropOverlayEl.hidden = true;
}

function wireUploadUi() {
  uploadButtonEl.addEventListener("click", () => uploadInputEl.click());
  uploadInputEl.addEventListener("change", async () => {
    if (uploadInputEl.files && uploadInputEl.files.length > 0) {
      await addUploadedFiles(uploadInputEl.files);
      uploadInputEl.value = "";
    }
  });

  document.addEventListener("dragenter", (event) => {
    if (!hasFiles(event)) {
      return;
    }
    event.preventDefault();
    state.dragDepth += 1;
    showDropOverlay();
  });

  document.addEventListener("dragover", (event) => {
    if (!hasFiles(event)) {
      return;
    }
    event.preventDefault();
  });

  document.addEventListener("dragleave", (event) => {
    if (!hasFiles(event)) {
      return;
    }
    event.preventDefault();
    state.dragDepth = Math.max(0, state.dragDepth - 1);
    if (state.dragDepth === 0) {
      hideDropOverlay();
    }
  });

  document.addEventListener("drop", async (event) => {
    if (!hasFiles(event)) {
      return;
    }
    event.preventDefault();
    state.dragDepth = 0;
    hideDropOverlay();
    await addUploadedFiles(event.dataTransfer.files);
  });
}

async function main() {
  wireUploadUi();
  galleryEl.innerHTML = "<p class=\"is-loading\">Loading built-in themes…</p>";

  try {
    state.themes = await loadBuiltinThemes();
    renderGallery();
    setStatus(`Loaded ${state.themes.length} built-in themes. Drop or upload another YAML file to preview it here.`, "success");
  } catch (error) {
    console.error(error);
    galleryEl.innerHTML = `
      <p class="is-error">
        Failed to load built-in themes from <code>/resources/themes/</code>. Run <code>just theme-preview-run</code> so the SPA can see the mounted theme directory.
      </p>
    `;
    setStatus("Failed to load built-in themes.", "error");
  }
}

main();
