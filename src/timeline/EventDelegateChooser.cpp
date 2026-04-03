// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EventDelegateChooser.h"
#include "EventDataSource.h"

#include "logging/Logging.h"

#include <QJSValue>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QtGlobal>

#include <ranges>

// privat qt headers to access required properties
#include <QtQml/private/qqmlincubator_p.h>
#include <QtQml/private/qqmlobjectcreator_p.h>

namespace {

bool
churnPerfEnabled()
{
    static const bool enabled = [] {
        auto val = qgetenv("KOMAI_PERF_TIMELINE_CHURN").trimmed().toLower();
        return val == "1" || val == "true" || val == "yes" || val == "on";
    }();
    return enabled;
}

int
lookupTypeRole(const QAbstractItemModel *model)
{
    if (!model)
        return -1;

    const auto names = model->roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        if (it.value() == "type")
            return it.key();
    }

    return -1;
}

QVariant
readPreviewProperty(const QVariant &previewData, const QString &propertyName)
{
    if (!previewData.isValid())
        return {};

    if (previewData.canConvert<QVariantMap>()) {
        const auto previewMap = previewData.toMap();
        if (const auto it = previewMap.find(propertyName); it != previewMap.end())
            return it.value();
    } else if (previewData.userType() == qMetaTypeId<QJSValue>()) {
        const auto previewMap = previewData.value<QJSValue>().toVariant().toMap();
        if (const auto it = previewMap.find(propertyName); it != previewMap.end())
            return it.value();
    } else if (auto *previewObject = qvariant_cast<QObject *>(previewData); previewObject) {
        const auto propertyNameUtf8 = propertyName.toUtf8();
        if (previewObject->metaObject()->indexOfProperty(propertyNameUtf8.constData()) >= 0)
            return previewObject->property(propertyNameUtf8.constData());
    }

    return {};
}

} // namespace

QQmlComponent *
EventDelegateChoice::delegate() const
{
    return delegate_;
}

void
EventDelegateChoice::setDelegate(QQmlComponent *delegate)
{
    if (delegate != delegate_) {
        delegate_ = delegate;
        emit delegateChanged();
        emit changed();
    }
}

QList<int>
EventDelegateChoice::roleValues() const
{
    return roleValues_;
}

void
EventDelegateChoice::setRoleValues(const QList<int> &value)
{
    if (value != roleValues_) {
        roleValues_ = value;
        emit roleValuesChanged();
        emit changed();
    }
}

QQmlListProperty<EventDelegateChoice>
EventDelegateChooser::choices()
{
    return QQmlListProperty<EventDelegateChoice>(this,
                                                 this,
                                                 &EventDelegateChooser::appendChoice,
                                                 &EventDelegateChooser::choiceCount,
                                                 &EventDelegateChooser::choice,
                                                 &EventDelegateChooser::clearChoices);
}

void
EventDelegateChooser::appendChoice(QQmlListProperty<EventDelegateChoice> *p, EventDelegateChoice *c)
{
    EventDelegateChooser *dc = static_cast<EventDelegateChooser *>(p->object);
    dc->choices_.append(c);
}

qsizetype
EventDelegateChooser::choiceCount(QQmlListProperty<EventDelegateChoice> *p)
{
    return static_cast<EventDelegateChooser *>(p->object)->choices_.count();
}
EventDelegateChoice *
EventDelegateChooser::choice(QQmlListProperty<EventDelegateChoice> *p, qsizetype index)
{
    return static_cast<EventDelegateChooser *>(p->object)->choices_.at(index);
}
void
EventDelegateChooser::clearChoices(QQmlListProperty<EventDelegateChoice> *p)
{
    static_cast<EventDelegateChooser *>(p->object)->choices_.clear();
}

void
EventDelegateChooser::componentComplete()
{
    QQuickItem::componentComplete();
    eventIncubator.reset(eventId_);
    replyIncubator.reset(replyId);
    // eventIncubator.forceCompletion();
}

QVariant
EventDelegateChooser::DelegateIncubator::readPreviewValue(const char *propertyName) const
{
    auto key = QString::fromUtf8(propertyName);
    auto previewData =
      forReply ? chooser.property("replyPreviewData") : chooser.property("previewData");
    if (!previewData.isValid() && forReply)
        previewData = chooser.property("previewData");

    if (const auto previewValue = readPreviewProperty(previewData, key); previewValue.isValid())
        return previewValue;

    auto chooserMeta = chooser.metaObject();
    if (chooserMeta->indexOfProperty(propertyName) >= 0) {
        auto directValue = chooser.property(propertyName);
        if (directValue.isValid())
            return directValue;
    }

    auto chooserContext = QQmlEngine::contextForObject(&chooser);
    if (!chooserContext)
        return {};

    auto contextValue = chooserContext->contextProperty(key);
    if (contextValue.isValid())
        return contextValue;

    auto modelData = chooserContext->contextProperty(QStringLiteral("modelData"));
    if (modelData.canConvert<QVariantMap>()) {
        auto modelMap = modelData.toMap();
        if (auto it = modelMap.find(key); it != modelMap.end())
            return it.value();
    }

    auto evaluateInContext = [chooserContext, this](const QString &expressionText) -> QVariant {
        QQmlExpression expr(
          chooserContext, const_cast<EventDelegateChooser *>(&chooser), expressionText);
        auto value = expr.evaluate();
        return expr.hasError() ? QVariant{} : value;
    };

    if (auto value = evaluateInContext(key); value.isValid())
        return value;

    if (auto value = evaluateInContext(QStringLiteral("modelData.%1").arg(key)); value.isValid())
        return value;

    if (auto value = evaluateInContext(QStringLiteral("model.%1").arg(key)); value.isValid())
        return value;

    return {};
}

bool
EventDelegateChooser::DelegateIncubator::refreshRoomlessProperties()
{
    auto *obj = object();
    if (!obj || chooser.room_)
        return false;

    // Read the preview data map directly — only keys present in this map
    // should be written to the delegate.  The full readPreviewValue() fallback
    // chain (context properties, expression evaluation) is too broad and would
    // overwrite built-in QQuickItem properties like parent/x/y/width/visible.
    QVariant previewData =
      forReply ? chooser.property("replyPreviewData") : chooser.property("previewData");
    if (!previewData.isValid() && forReply)
        previewData = chooser.property("previewData");

    QVariantMap previewMap;
    if (previewData.canConvert<QVariantMap>()) {
        previewMap = previewData.toMap();
    } else if (previewData.userType() == qMetaTypeId<QJSValue>()) {
        previewMap = previewData.value<QJSValue>().toVariant().toMap();
    }
    if (previewMap.isEmpty())
        return false;

    auto *mo = obj->metaObject();

    Qt::beginPropertyUpdateGroup();
    for (auto it = previewMap.cbegin(); it != previewMap.cend(); ++it) {
        const auto &value = it.value();
        if (!value.isValid())
            continue;

        int propIdx = mo->indexOfProperty(it.key().toUtf8().constData());
        if (propIdx < 0)
            continue;

        auto prop = mo->property(propIdx);
        if (!prop.isWritable())
            continue;

        prop.write(obj, value);
    }
    Qt::endPropertyUpdateGroup();

    chooser.polish();
    return true;
}

void
EventDelegateChooser::DelegateIncubator::setInitialState(QObject *obj)
{
    auto item = qobject_cast<QQuickItem *>(obj);
    if (!item)
        return;

    item->setParentItem(&chooser);
    item->setParent(&chooser);

    auto attached = qobject_cast<EventDelegateChooserAttachedType *>(
      qmlAttachedPropertiesObject<EventDelegateChooser>(obj));
    Q_ASSERT(attached != nullptr);
    attached->setIsReply(this->forReply || chooser.limitAsReply_);

    auto chooserContext = QQmlEngine::contextForObject(&chooser);
    if (!chooserContext)
        chooserContext = QQmlEngine::contextForObject(obj);
    // Workaround for https://bugreports.qt.io/browse/QTBUG-98846
    QHash<QString, RequiredPropertyKey> requiredProperties;
    for (const auto &[propKey, prop] :
         QQmlIncubatorPrivate::get(this)->requiredProperties()->asKeyValueRange()) {
        requiredProperties.insert(prop.propertyName, propKey);
    }

    auto mo = obj->metaObject();

    // Preview delegates may be instantiated without a room-backed event data source.
    // In that case, populate required properties from the delegate context.
    if (!chooser.room_) {
        Qt::beginPropertyUpdateGroup();
        for (int i = 0; i < mo->propertyCount(); i++) {
            auto prop = mo->property(i);
            if (!prop.isRequired() && !requiredProperties.contains(prop.name()))
                continue;

            auto value = readPreviewValue(prop.name());
            if (!value.isValid())
                continue;

            mo->property(i).write(obj, value);
            if (const auto &req = requiredProperties.find(prop.name());
                req != requiredProperties.end())
                QQmlIncubatorPrivate::get(this)->requiredProperties()->remove(*req);
        }
        Qt::endPropertyUpdateGroup();
        return;
    }

    auto roleNames = chooser.room_->roleNames();
    QHash<QByteArray, int> nameToRole;
    for (const auto &[k, v] : roleNames.asKeyValueRange()) {
        nameToRole.insert(v, k);
    }

    QHash<int, int> roleToPropIdx;
    std::vector<QModelRoleData> roles;
    for (int i = 0; i < mo->propertyCount(); i++) {
        auto prop = mo->property(i);
        if (!prop.isRequired() && !requiredProperties.contains(prop.name()))
            continue;

        if (auto role = nameToRole.find(prop.name()); role != nameToRole.end()) {
            roleToPropIdx.insert(*role, i);
            roles.emplace_back(*role);
        } else {
            nhlog::ui()->critical("Required property {} not found in model!", prop.name());
        }
    }

    chooser.room_->multiData(currentId, forReply ? chooser.eventId_ : QString(), roles);

    Qt::beginPropertyUpdateGroup();
    for (const auto &role : roles) {
        const auto &roleName = roleNames[role.role()];
        // nhlog::ui()->critical("Setting role {}, {} to {}",
        //                       role.role(),
        //                       roleName.toStdString(),
        //                       role.data().toString().toStdString());

        // nhlog::ui()->critical("Setting {}", mo->property(roleToPropIdx[role.role()]).name());
        mo->property(roleToPropIdx[role.role()]).write(obj, role.data());

        if (const auto &req = requiredProperties.find(roleName); req != requiredProperties.end())
            QQmlIncubatorPrivate::get(this)->requiredProperties()->remove(*req);
    }

    Qt::endPropertyUpdateGroup();

    // setInitialProperties(rolesToSet);

    const int typeRoleId = lookupTypeRole(chooser.room_);

    auto update = [this, obj, roleToPropIdx = std::move(roleToPropIdx), typeRoleId](
                    const QList<int> &changedRoles, EventDataSource *room) {
        if (!room)
            return;

        if (typeRoleId >= 0 && (changedRoles.empty() || changedRoles.contains(typeRoleId))) {
            int type =
              room->dataById(currentId, typeRoleId, forReply ? chooser.eventId_ : QString())
                .toInt();
            if (type != oldType) {
                if (churnPerfEnabled()) {
                    nhlog::ui()->info("[churn] typeChangeReset chooser={} event={} forReply={} "
                                      "oldType={} newType={} changedRolesCount={}",
                                      (void *)&chooser,
                                      currentId.toStdString(),
                                      forReply,
                                      oldType,
                                      type,
                                      changedRoles.size());
                }
                reset(currentId);
                return;
            }
        }

        std::vector<QModelRoleData> rolesToRequest;

        if (changedRoles.empty()) {
            for (const auto role :
                 std::ranges::subrange(roleToPropIdx.keyBegin(), roleToPropIdx.keyEnd()))
                rolesToRequest.emplace_back(role);
        } else {
            for (auto role : changedRoles) {
                if (roleToPropIdx.contains(role)) {
                    rolesToRequest.emplace_back(role);
                }
            }
        }

        if (rolesToRequest.empty())
            return;

        auto mo = obj->metaObject();
        room->multiData(currentId, forReply ? chooser.eventId_ : QString(), rolesToRequest);

        Qt::beginPropertyUpdateGroup();
        for (const auto &role : rolesToRequest) {
            mo->property(roleToPropIdx[role.role()]).write(obj, role.data());
        }
        Qt::endPropertyUpdateGroup();
    };

    const auto trackedId      = currentId;
    const auto trackedEventId = chooser.eventId_;
    auto connection           = connect(
      chooser.room_,
      &QAbstractItemModel::dataChanged,
      obj,
      [trackedId, trackedEventId, update, room = chooser.room_, isReply = forReply](
        const QModelIndex &topLeft,
        const QModelIndex &bottomRight,
        const QList<int> &changedRoles) {
          if (!room)
              return;

          auto rowInChangedRange = [&topLeft, &bottomRight, room](const QString &eventId) {
              const auto row = room->idToIndex(eventId);
              return row >= topLeft.row() && row <= bottomRight.row();
          };

          if (!isReply) {
              if (!rowInChangedRange(trackedId))
                  return;
          } else if (!rowInChangedRange(trackedId) && !rowInChangedRange(trackedEventId)) {
              return;
          }

          update(changedRoles, room);
      },
      Qt::QueuedConnection);
    connect(
      &this->chooser,
      &EventDelegateChooser::destroyed,
      obj,
      [connection]() { QObject::disconnect(connection); },
      Qt::SingleShotConnection);
    connect(
      &this->chooser,
      &EventDelegateChooser::roomChanged,
      obj,
      [connection]() { QObject::disconnect(connection); },
      Qt::SingleShotConnection);
}

void
EventDelegateChooser::DelegateIncubator::reset(QString id)
{
    if (id.isEmpty())
        return;

    this->currentId = id;

    int role = -1;
    if (chooser.room_) {
        const int typeRoleId = lookupTypeRole(chooser.room_);
        if (typeRoleId >= 0)
            role = chooser.room_->dataById(id, typeRoleId, forReply ? chooser.eventId_ : QString())
                     .toInt();
    } else {
        QVariant roleValue;
        auto previewData = chooser.property(forReply ? "replyPreviewData" : "previewData");
        if (!previewData.isValid() && forReply)
            previewData = chooser.property("previewData");
        roleValue = readPreviewProperty(previewData, QStringLiteral("type"));

        if (!roleValue.isValid())
            roleValue = chooser.property("type");
        if (!roleValue.isValid()) {
            if (auto chooserContext = QQmlEngine::contextForObject(&chooser)) {
                roleValue = chooserContext->contextProperty(QStringLiteral("type"));
                if (!roleValue.isValid()) {
                    QQmlExpression expr(chooserContext, &chooser, QStringLiteral("type"));
                    roleValue = expr.evaluate();
                }
            }
        }

        role = roleValue.toInt();
    }

    if (churnPerfEnabled()) {
        nhlog::ui()->info("[churn] reset chooser={} event={} forReply={} hadObject={} "
                          "oldType={} newType={} hasRoom={}",
                          (void *)&chooser,
                          id.toStdString(),
                          forReply,
                          object() != nullptr,
                          oldType,
                          role,
                          chooser.room_ != nullptr);
    }

    // For roomless delegates, if the type hasn't changed and we already have
    // a delegate, update its properties in-place instead of destroying and
    // recreating the entire component.
    if (!chooser.room_ && role == oldType && object()) {
        if (churnPerfEnabled()) {
            nhlog::ui()->info("[churn] refreshInPlace chooser={} event={} forReply={} type={}",
                              (void *)&chooser,
                              id.toStdString(),
                              forReply,
                              role);
        }
        refreshRoomlessProperties();
        return;
    }

    this->oldType = role;

    for (const auto choice : std::as_const(chooser.choices_)) {
        const auto &choiceValue = choice->roleValues();
        if (choiceValue.contains(role) || choiceValue.empty()) {
            if (auto child = qobject_cast<QQuickItem *>(object())) {
                child->setParentItem(nullptr);
            }

            choice->delegate()->create(*this, QQmlEngine::contextForObject(&chooser));
            return;
        }
    }
}

void
EventDelegateChooser::DelegateIncubator::statusChanged(QQmlIncubator::Status status)
{
    if (churnPerfEnabled()) {
        nhlog::ui()->info("[churn] statusChanged chooser={} event={} forReply={} status={}",
                          (void *)&chooser,
                          currentId.toStdString(),
                          forReply,
                          static_cast<int>(status));
    }

    if (status == QQmlIncubator::Ready) {
        auto child = qobject_cast<QQuickItem *>(object());
        if (child == nullptr) {
            nhlog::ui()->error("Delegate has to be derived of Item!");
            return;
        }

        child->setParentItem(&chooser);
        QQmlEngine::setObjectOwnership(child, QQmlEngine::ObjectOwnership::CppOwnership);

        // connect(child, &QQuickItem::parentChanged, child, [child](QQuickItem *) {
        //     // QTBUG-115687
        //     if (child->flags().testFlag(QQuickItem::ItemObservesViewport)) {
        //         nhlog::ui()->critical("SETTING OBSERVES VIEWPORT");
        //         // Re-trigger the parent traversal to get subtreeTransformChangedEnabled turned
        //         on child->setFlag(QQuickItem::ItemObservesViewport);
        //     }
        // });

        if (forReply)
            emit chooser.replyChanged();
        else
            emit chooser.mainChanged();

        chooser.polish();
    } else if (status == QQmlIncubator::Error) {
        auto errors_ = errors();
        for (const auto &e : std::as_const(errors_))
            nhlog::ui()->error("Error instantiating delegate: {}", e.toString().toStdString());
    }
}

void
EventDelegateChooser::updatePolish()
{
    auto mainChild  = qobject_cast<QQuickItem *>(eventIncubator.object());
    auto replyChild = qobject_cast<QQuickItem *>(replyIncubator.object());

    if (churnPerfEnabled()) {
        nhlog::ui()->info("[churn] polish chooser={} event={} hasMain={} hasReply={} "
                          "maxWidth={} width={}",
                          (void *)this,
                          eventId_.toStdString(),
                          mainChild != nullptr,
                          replyChild != nullptr,
                          maxWidth_,
                          qRound(width()));
    }

    auto layoutItem = [this](QQuickItem *item, int inset) {
        if (item) {
            QObject::disconnect(item, &QQuickItem::implicitWidthChanged, this, &QQuickItem::polish);

            auto attached = qobject_cast<EventDelegateChooserAttachedType *>(
              qmlAttachedPropertiesObject<EventDelegateChooser>(item));
            Q_ASSERT(attached != nullptr);

            int maxWidth = maxWidth_ - inset;

            // in theory we could also reset the width, but that doesn't seem to work nicely for
            // text areas because of how they cache it.
            if (attached->maxWidth() > 0)
                item->setWidth(attached->maxWidth());
            else
                item->setWidth(maxWidth);
            item->ensurePolished();
            auto width = item->implicitWidth();

            if (width < 1 || width > maxWidth)
                width = maxWidth;

            if (attached->maxWidth() > 0 && width > attached->maxWidth())
                width = attached->maxWidth();

            if (attached->keepAspectRatio()) {
                auto height = width * attached->aspectRatio();
                if (attached->maxHeight() && height > attached->maxHeight()) {
                    height = attached->maxHeight();
                    width  = height / attached->aspectRatio();
                }

                item->setHeight(height);
            }

            item->setWidth(width);
            item->ensurePolished();

            QObject::connect(item, &QQuickItem::implicitWidthChanged, this, &QQuickItem::polish);
        }
    };

    layoutItem(mainChild, mainInset_);
    layoutItem(replyChild, replyInset_);
}

void
EventDelegateChooserAttachedType::polishChooser()
{
    auto p = parent();
    if (p) {
        auto chooser = qobject_cast<EventDelegateChooser *>(p->parent());
        if (chooser) {
            chooser->polish();
        }
    }
}

#include "moc_EventDelegateChooser.cpp"
