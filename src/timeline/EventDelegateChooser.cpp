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
#include <QQmlProperty>
#include <QSet>
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

QVariantMap
previewDataToMap(const QVariant &previewData)
{
    if (!previewData.isValid())
        return {};

    if (previewData.canConvert<QVariantMap>())
        return previewData.toMap();

    if (previewData.userType() == qMetaTypeId<QJSValue>())
        return previewData.value<QJSValue>().toVariant().toMap();

    return {};
}

QStringList
requiredDelegatePropertyNames(QObject *obj,
                              const QHash<QString, QList<RequiredPropertyKey>> &requiredProperties)
{
    QStringList propertyNames;
    QSet<QString> seen;
    const auto *mo = obj->metaObject();

    for (int i = 0; i < mo->propertyCount(); ++i) {
        const auto prop         = mo->property(i);
        const auto propertyName = QString::fromUtf8(prop.name());
        if (!prop.isRequired() && !requiredProperties.contains(propertyName))
            continue;
        if (seen.contains(propertyName))
            continue;

        seen.insert(propertyName);
        propertyNames.push_back(propertyName);
    }

    for (auto it = requiredProperties.cbegin(); it != requiredProperties.cend(); ++it) {
        if (seen.contains(it.key()))
            continue;

        seen.insert(it.key());
        propertyNames.push_back(it.key());
    }

    return propertyNames;
}

void
removeRequiredPropertyKeys(QQmlIncubator *incubator,
                           const QHash<QString, QList<RequiredPropertyKey>> &requiredProperties,
                           const QString &propertyName)
{
    if (const auto it = requiredProperties.find(propertyName); it != requiredProperties.end()) {
        for (const auto &key : it.value())
            QQmlIncubatorPrivate::get(incubator)->requiredProperties()->remove(key);
    }
}

bool
writeResolvedProperty(QObject *obj,
                      const QString &propertyName,
                      const QVariant &value,
                      QQmlContext *fallbackContext)
{
    QQmlProperty property = fallbackContext ? QQmlProperty(obj, propertyName, fallbackContext)
                                            : QQmlProperty(obj, propertyName);
    return property.isValid() && property.isWritable() && property.write(value);
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
    previewMap = previewDataToMap(previewData);
    if (previewMap.isEmpty())
        return false;

    auto *context = QQmlEngine::contextForObject(obj);
    if (!context)
        context = QQmlEngine::contextForObject(&chooser);

    Qt::beginPropertyUpdateGroup();
    for (auto it = previewMap.cbegin(); it != previewMap.cend(); ++it) {
        const auto &value = it.value();
        if (!value.isValid())
            continue;

        if (!writeResolvedProperty(obj, it.key(), value, context))
            continue;
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

    auto *objectContext  = QQmlEngine::contextForObject(obj);
    auto *chooserContext = QQmlEngine::contextForObject(&chooser);
    if (!chooserContext)
        chooserContext = objectContext;
    // Workaround for https://bugreports.qt.io/browse/QTBUG-98846
    QHash<QString, QList<RequiredPropertyKey>> requiredProperties;
    for (const auto &[propKey, prop] :
         QQmlIncubatorPrivate::get(this)->requiredProperties()->asKeyValueRange()) {
        if (propKey.object != obj)
            continue;
        requiredProperties[prop.propertyName].push_back(propKey);
    }

    const auto propertyNames = requiredDelegatePropertyNames(obj, requiredProperties);

    // Preview delegates may be instantiated without a room-backed event data source.
    // In that case, populate required properties from the delegate context.
    if (!chooser.room_) {
        QVariant previewData =
          forReply ? chooser.property("replyPreviewData") : chooser.property("previewData");
        if (!previewData.isValid() && forReply)
            previewData = chooser.property("previewData");
        const auto previewMap = previewDataToMap(previewData);
        QSet<QString> writtenProperties;

        Qt::beginPropertyUpdateGroup();
        for (auto it = previewMap.cbegin(); it != previewMap.cend(); ++it) {
            if (!it.value().isValid())
                continue;

            if (!writeResolvedProperty(
                  obj, it.key(), it.value(), objectContext ? objectContext : chooserContext))
                continue;

            writtenProperties.insert(it.key());
            removeRequiredPropertyKeys(this, requiredProperties, it.key());
        }

        for (const auto &propertyName : propertyNames) {
            if (writtenProperties.contains(propertyName))
                continue;

            const auto propertyNameUtf8 = propertyName.toUtf8();
            auto value                  = readPreviewValue(propertyNameUtf8.constData());
            if (!value.isValid())
                continue;

            if (!writeResolvedProperty(
                  obj, propertyName, value, objectContext ? objectContext : chooserContext))
                continue;

            removeRequiredPropertyKeys(this, requiredProperties, propertyName);
        }
        Qt::endPropertyUpdateGroup();
        return;
    }

    auto roleNames = chooser.room_->roleNames();
    QHash<QByteArray, int> nameToRole;
    for (const auto &[k, v] : roleNames.asKeyValueRange()) {
        nameToRole.insert(v, k);
    }

    QHash<int, QString> roleToPropertyName;
    std::vector<QModelRoleData> roles;
    auto *propertyContext = objectContext ? objectContext : chooserContext;
    for (const auto &propertyName : propertyNames) {
        const auto propertyNameUtf8 = propertyName.toUtf8();
        if (auto role = nameToRole.find(propertyNameUtf8); role != nameToRole.end()) {
            roleToPropertyName.insert(*role, propertyName);
            roles.emplace_back(*role);
        } else {
            komai::logging::ui()->critical("Required property {} not found in model!",
                                           propertyName.toStdString());
        }
    }

    for (const auto &[role, roleName] : roleNames.asKeyValueRange()) {
        if (roleToPropertyName.contains(role))
            continue;

        const auto propertyName = QString::fromUtf8(roleName);
        const auto property     = propertyContext ? QQmlProperty(obj, propertyName, propertyContext)
                                                  : QQmlProperty(obj, propertyName);
        if (!property.isProperty() || !property.isValid() || !property.isWritable())
            continue;

        roleToPropertyName.insert(role, propertyName);
        roles.emplace_back(role);
    }

    chooser.room_->multiData(currentId, forReply ? chooser.eventId_ : QString(), roles);

    Qt::beginPropertyUpdateGroup();
    for (const auto &role : roles) {
        const auto propertyName = roleToPropertyName.value(role.role());
        writeResolvedProperty(obj, propertyName, role.data(), propertyContext);

        removeRequiredPropertyKeys(this, requiredProperties, propertyName);
    }

    Qt::endPropertyUpdateGroup();

    // setInitialProperties(rolesToSet);

    const int typeRoleId = lookupTypeRole(chooser.room_);

    auto update =
      [this, obj, propertyContext, roleToPropertyName = std::move(roleToPropertyName), typeRoleId](
        const QList<int> &changedRoles, EventDataSource *room) {
          if (!room)
              return;

          if (typeRoleId >= 0 && (changedRoles.empty() || changedRoles.contains(typeRoleId))) {
              int type =
                room->dataById(currentId, typeRoleId, forReply ? chooser.eventId_ : QString())
                  .toInt();
              if (type != oldType) {
                  if (churnPerfEnabled()) {
                      komai::logging::ui()->info(
                        "[churn] typeChangeReset chooser={} event={} forReply={} "
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
              for (const auto role : std::ranges::subrange(roleToPropertyName.keyBegin(),
                                                           roleToPropertyName.keyEnd()))
                  rolesToRequest.emplace_back(role);
          } else {
              for (auto role : changedRoles) {
                  if (roleToPropertyName.contains(role)) {
                      rolesToRequest.emplace_back(role);
                  }
              }
          }

          if (rolesToRequest.empty())
              return;

          room->multiData(currentId, forReply ? chooser.eventId_ : QString(), rolesToRequest);

          Qt::beginPropertyUpdateGroup();
          for (const auto &role : rolesToRequest) {
              writeResolvedProperty(
                obj, roleToPropertyName.value(role.role()), role.data(), propertyContext);
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

    int role         = -1;
    bool foundInRoom = false;
    if (chooser.room_) {
        const int typeRoleId = lookupTypeRole(chooser.room_);
        if (typeRoleId >= 0) {
            const auto typeValue =
              chooser.room_->dataById(id, typeRoleId, forReply ? chooser.eventId_ : QString());
            if (typeValue.isValid()) {
                foundInRoom = true;
                role        = typeValue.toInt();
            }
        }
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
        komai::logging::ui()->info("[churn] reset chooser={} event={} forReply={} hadObject={} "
                                   "oldType={} newType={} hasRoom={} foundInRoom={}",
                                   (void *)&chooser,
                                   id.toStdString(),
                                   forReply,
                                   object() != nullptr,
                                   oldType,
                                   role,
                                   chooser.room_ != nullptr,
                                   foundInRoom);
    }

    // Stale-delegate guard: if the chooser has a room but the event isn't in
    // it, `eventId_` is a leftover from a previous model — the delegate's
    // required-property update hasn't caught up with the view's model swap
    // (e.g. thread↔per-room transition). Skip the fallback so we don't
    // flash "Unsupported"; the ListView will release or rebind the delegate
    // shortly, and setEventId will trigger a proper reset then.
    if (chooser.room_ && !foundInRoom)
        return;

    // For roomless delegates, if the type hasn't changed and we already have
    // a delegate, update its properties in-place instead of destroying and
    // recreating the entire component.
    if (!chooser.room_ && role == oldType && object()) {
        if (churnPerfEnabled()) {
            komai::logging::ui()->info(
              "[churn] refreshInPlace chooser={} event={} forReply={} type={}",
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
        komai::logging::ui()->info(
          "[churn] statusChanged chooser={} event={} forReply={} status={}",
          (void *)&chooser,
          currentId.toStdString(),
          forReply,
          static_cast<int>(status));
    }

    if (status == QQmlIncubator::Ready) {
        auto child = qobject_cast<QQuickItem *>(object());
        if (child == nullptr) {
            komai::logging::ui()->error("Delegate has to be derived of Item!");
            return;
        }

        child->setParentItem(&chooser);
        QQmlEngine::setObjectOwnership(child, QQmlEngine::ObjectOwnership::CppOwnership);

        // connect(child, &QQuickItem::parentChanged, child, [child](QQuickItem *) {
        //     // QTBUG-115687
        //     if (child->flags().testFlag(QQuickItem::ItemObservesViewport)) {
        //         komai::logging::ui()->critical("SETTING OBSERVES VIEWPORT");
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
            komai::logging::ui()->error("Error instantiating delegate: {}",
                                        e.toString().toStdString());
    }
}

void
EventDelegateChooser::updatePolish()
{
    auto mainChild  = qobject_cast<QQuickItem *>(eventIncubator.object());
    auto replyChild = qobject_cast<QQuickItem *>(replyIncubator.object());

    if (churnPerfEnabled()) {
        komai::logging::ui()->info("[churn] polish chooser={} event={} hasMain={} hasReply={} "
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
