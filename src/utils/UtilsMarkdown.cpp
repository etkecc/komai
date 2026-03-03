// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Utils.h"

#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QTextBoundaryFinder>

#include <cmark.h>

namespace {
void
rainbowify(cmark_node *node)
{
    // create iterator over node
    cmark_iter *iter = cmark_iter_new(node);

    // First loop to get total text length
    int textLen = 0;
    while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);
        // only text nodes (no code or semilar)
        if (cmark_node_get_type(cur) != CMARK_NODE_TEXT)
            continue;
        // count up by length of current node's text
        QTextBoundaryFinder tbf(QTextBoundaryFinder::BoundaryType::Grapheme,
                                QString(cmark_node_get_literal(cur)));
        while (tbf.toNextBoundary() != -1)
            textLen++;
    }

    // create new iter to start over
    cmark_iter_free(iter);
    iter = cmark_iter_new(node);

    // Second loop to rainbowify
    int charIdx = 0;
    while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);
        // only text nodes (no code or similar)
        if (cmark_node_get_type(cur) != CMARK_NODE_TEXT)
            continue;

        // get text in current node
        QString nodeText(cmark_node_get_literal(cur));
        // create buffer to append rainbow text to
        QString buf;
        int boundaryStart = 0;
        int boundaryEnd   = 0;
        // use QTextBoundaryFinder to iterate over graphemes
        QTextBoundaryFinder tbf(QTextBoundaryFinder::BoundaryType::Grapheme, nodeText);
        while ((boundaryEnd = tbf.toNextBoundary()) != -1) {
            charIdx++;
            // Split text to get current char
            auto curChar  = QStringView(nodeText).mid(boundaryStart, boundaryEnd - boundaryStart);
            boundaryStart = boundaryEnd;
            // Don't rainbowify whitespaces
            if (curChar.trimmed().isEmpty() || utils::codepointIsEmoji(curChar.toUcs4().at(0))) {
                buf.append(curChar);
                continue;
            }

            // get correct color for char index
            // Use colors as described here:
            // https://shark.comfsm.fm/~dleeling/cis/hsl_rainbow.html
            auto color = QColor::fromHslF(
              static_cast<float>((charIdx - 1.0) / textLen * (5. / 6.)), 0.9f, 0.5f);
            // format color for HTML
            auto colorString = color.name(QColor::NameFormat::HexRgb);
            // create HTML element for current char
            auto curCharColored =
              QStringLiteral("<font color=\"%0\">%1</font>").arg(colorString).arg(curChar);
            // append colored HTML element to buffer
            buf.append(curCharColored);
        }

        // create HTML_INLINE node to prevent HTML from being escaped
        auto htmlNode = cmark_node_new(CMARK_NODE_HTML_INLINE);
        // set content of HTML node to buffer contents
        cmark_node_set_literal(htmlNode, buf.toUtf8().data());
        // replace current node with HTML node
        cmark_node_replace(cur, htmlNode);
        // free memory of old node
        cmark_node_free(cur);
    }

    cmark_iter_free(iter);
}

std::string
extractSpoilerWarning(std::string &insideSpoiler)
{
    std::string spoiler_text;
    if (auto spoilerTextEnd = insideSpoiler.find("|"); spoilerTextEnd != std::string::npos) {
        spoiler_text  = insideSpoiler.substr(0, spoilerTextEnd);
        insideSpoiler = insideSpoiler.substr(spoilerTextEnd + 1);
    }
    return QString::fromStdString(spoiler_text).replace('"', "&quot;").toStdString();
}

// TODO(Nico): Add tests :D
void
processSpoilers(cmark_node *node)
{
    auto iter = cmark_iter_new(node);

    while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);

        // only text nodes (no code or similar)
        if (cmark_node_get_type(cur) != CMARK_NODE_TEXT) {
            continue;
        }

        std::string_view content = cmark_node_get_literal(cur);

        if (auto posStart = content.find("||"); posStart != std::string::npos) {
            // we have the start of the spoiler
            if (auto posEnd = content.find("||", posStart + 2); posEnd != std::string::npos) {
                // we have the end of the spoiler in the same node

                std::string before_spoiler = std::string(content.substr(0, posStart));
                std::string inside_spoiler =
                  std::string(content.substr(posStart + 2, posEnd - 2 - posStart));
                std::string after_spoiler = std::string(content.substr(posEnd + 2));

                std::string spoiler_text = extractSpoilerWarning(inside_spoiler);

                // create the new nodes
                auto before_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(before_node, before_spoiler.c_str());
                auto after_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(after_node, after_spoiler.c_str());

                auto block = cmark_node_new(cmark_node_type::CMARK_NODE_CUSTOM_INLINE);
                cmark_node_set_on_enter(
                  block, ("<span data-mx-spoiler=\"" + spoiler_text + "\">").c_str());
                cmark_node_set_on_exit(block, "</span>");
                auto child_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(child_node, inside_spoiler.c_str());
                cmark_node_append_child(block, child_node);

                // insert the new nodes into the tree
                cmark_node_replace(cur, block);
                cmark_node_insert_before(block, before_node);
                cmark_node_insert_after(block, after_node);

                // cleanup the replaced node
                cmark_node_free(cur);

                // fixup the iterator
                cmark_iter_reset(iter, block, CMARK_EVENT_EXIT);

            } else {
                // no end found, but lets try sibling nodes
                for (auto next = cmark_node_next(cur); next != nullptr;
                     next      = cmark_node_next(next)) {
                    // only text nodes again
                    if (cmark_node_get_type(next) != CMARK_NODE_TEXT)
                        continue;

                    std::string_view next_content = cmark_node_get_literal(next);
                    if (auto posEndNext = next_content.find("||");
                        posEndNext != std::string_view::npos) {
                        // We found the end of the spoiler
                        std::string before_spoiler = std::string(content.substr(0, posStart));
                        std::string after_spoiler =
                          std::string(next_content.substr(posEndNext + 2));

                        std::string inside_spoiler_start =
                          std::string(content.substr(posStart + 2));
                        std::string inside_spoiler_end =
                          std::string(next_content.substr(0, posEndNext));

                        std::string spoiler_text = extractSpoilerWarning(inside_spoiler_start);

                        // save all the nodes inside the spoiler for later
                        std::vector<cmark_node *> child_nodes;
                        for (auto kid = cmark_node_next(cur); kid != nullptr && kid != next;
                             kid      = cmark_node_next(kid)) {
                            child_nodes.push_back(kid);
                        }

                        // create the new nodes
                        auto before_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(before_node, before_spoiler.c_str());
                        auto after_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(after_node, after_spoiler.c_str());

                        auto block = cmark_node_new(cmark_node_type::CMARK_NODE_CUSTOM_INLINE);
                        cmark_node_set_on_enter(
                          block, ("<span data-mx-spoiler=\"" + spoiler_text + "\">").c_str());
                        cmark_node_set_on_exit(block, "</span>");

                        // create the content inside the spoiler by adding the old text at the
                        // start and the end as well as all the existing children
                        auto child_node_start = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(child_node_start, inside_spoiler_start.c_str());
                        cmark_node_append_child(block, child_node_start);
                        for (auto &child : child_nodes)
                            cmark_node_append_child(block, child);
                        auto child_node_end = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(child_node_end, inside_spoiler_end.c_str());
                        cmark_node_append_child(block, child_node_end);

                        // insert the new nodes into the tree
                        cmark_node_replace(cur, block);
                        cmark_node_insert_before(block, before_node);
                        cmark_node_insert_after(block, after_node);

                        // cleanup removed nodes
                        cmark_node_free(cur);
                        cmark_node_free(next);

                        // fixup the iterator
                        cmark_iter_reset(iter, block, CMARK_EVENT_EXIT);

                        break;
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
}

void
processStrikethrough(cmark_node *node)
{
    auto iter = cmark_iter_new(node);

    while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
        cmark_node *cur = cmark_iter_get_node(iter);

        // only text nodes (no code or similar)
        if (cmark_node_get_type(cur) != CMARK_NODE_TEXT) {
            continue;
        }

        std::string_view content = cmark_node_get_literal(cur);

        if (auto posStart = content.find("~~"); posStart != std::string::npos) {
            // we have the start of the strikethrough
            if (auto posEnd = content.find("~~", posStart + 2); posEnd != std::string::npos) {
                // we have the end of the strikethrough in the same node

                std::string before_strike = std::string(content.substr(0, posStart));
                std::string inside_strike =
                  std::string(content.substr(posStart + 2, posEnd - 2 - posStart));
                std::string after_strike = std::string(content.substr(posEnd + 2));

                // create the new nodes
                auto before_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(before_node, before_strike.c_str());
                auto after_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(after_node, after_strike.c_str());

                auto block = cmark_node_new(cmark_node_type::CMARK_NODE_CUSTOM_INLINE);
                cmark_node_set_on_enter(block, "<del>");
                cmark_node_set_on_exit(block, "</del>");
                auto child_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                cmark_node_set_literal(child_node, inside_strike.c_str());
                cmark_node_append_child(block, child_node);

                // insert the new nodes into the tree
                cmark_node_replace(cur, block);
                cmark_node_insert_before(block, before_node);
                cmark_node_insert_after(block, after_node);

                // cleanup the replaced node
                cmark_node_free(cur);

                // fixup the iterator
                cmark_iter_reset(iter, block, CMARK_EVENT_EXIT);

            } else {
                // no end found, but lets try sibling nodes
                for (auto next = cmark_node_next(cur); next != nullptr;
                     next      = cmark_node_next(next)) {
                    // only text nodes again
                    if (cmark_node_get_type(next) != CMARK_NODE_TEXT)
                        continue;

                    std::string_view next_content = cmark_node_get_literal(next);
                    if (auto posEndNext = next_content.find("~~");
                        posEndNext != std::string_view::npos) {
                        // We found the end of the strikethrough
                        std::string before_strike = std::string(content.substr(0, posStart));
                        std::string after_strike = std::string(next_content.substr(posEndNext + 2));

                        std::string inside_strike_start = std::string(content.substr(posStart + 2));
                        std::string inside_strike_end =
                          std::string(next_content.substr(0, posEndNext));

                        // save all the nodes inside the strikethrough for later
                        std::vector<cmark_node *> child_nodes;
                        for (auto kid = cmark_node_next(cur); kid != nullptr && kid != next;
                             kid      = cmark_node_next(kid)) {
                            child_nodes.push_back(kid);
                        }

                        // create the new nodes
                        auto before_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(before_node, before_strike.c_str());
                        auto after_node = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(after_node, after_strike.c_str());

                        auto block = cmark_node_new(cmark_node_type::CMARK_NODE_CUSTOM_INLINE);
                        cmark_node_set_on_enter(block, "<del>");
                        cmark_node_set_on_exit(block, "</del>");

                        // create the content inside the strikethrough by adding the old text at the
                        // start and the end as well as all the existing children
                        auto child_node_start = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(child_node_start, inside_strike_start.c_str());
                        cmark_node_append_child(block, child_node_start);
                        for (auto &child : child_nodes)
                            cmark_node_append_child(block, child);
                        auto child_node_end = cmark_node_new(cmark_node_type::CMARK_NODE_TEXT);
                        cmark_node_set_literal(child_node_end, inside_strike_end.c_str());
                        cmark_node_append_child(block, child_node_end);

                        // insert the new nodes into the tree
                        cmark_node_replace(cur, block);
                        cmark_node_insert_before(block, before_node);
                        cmark_node_insert_after(block, after_node);

                        // cleanup removed nodes
                        cmark_node_free(cur);
                        cmark_node_free(next);

                        // fixup the iterator
                        cmark_iter_reset(iter, block, CMARK_EVENT_EXIT);

                        break;
                    }
                }
            }
        }
    }

    cmark_iter_free(iter);
}
} // namespace

QString
utils::markdownToHtml(const QString &text, bool rainbowify_, bool noExtensions)
{
    const auto str         = text.toUtf8();
    cmark_node *const node = cmark_parse_document(str.constData(), str.size(), CMARK_OPT_UNSAFE);

    if (!noExtensions) {
        processStrikethrough(node);
        processSpoilers(node);

        if (rainbowify_) {
            rainbowify(node);
        }
    }

    const char *tmp_buf = cmark_render_html(
      node,
      // by default make single linebreaks <br> tags
      noExtensions ? CMARK_OPT_UNSAFE : (CMARK_OPT_UNSAFE | CMARK_OPT_HARDBREAKS));
    // Copy the null terminated output buffer.
    std::string html(tmp_buf);

    // The buffer is no longer needed.
    free((char *)tmp_buf);
    cmark_node_free(node);

    auto result = escapeBlacklistedHtml(QString::fromStdString(html)).trimmed();

    if (!noExtensions) {
        result = linkifyMessage(std::move(result)).trimmed();
    }

    if (result.count(QStringLiteral("<p>")) == 1 && result.startsWith(QLatin1String("<p>")) &&
        result.endsWith(QLatin1String("</p>"))) {
        result = result.mid(3, result.size() - 3 - 4);
    }

    return result;
}
