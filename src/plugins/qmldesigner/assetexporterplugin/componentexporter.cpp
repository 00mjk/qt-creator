/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/
#include "componentexporter.h"
#include "parsers/modelnodeparser.h"

#include "model.h"
#include "nodeabstractproperty.h"
#include "rewriterview.h"

#include "utils/qtcassert.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

namespace {
Q_LOGGING_CATEGORY(loggerInfo, "qtc.designer.assetExportPlugin.modelExporter", QtInfoMsg)

static void populateLineage(const QmlDesigner::ModelNode &node, QByteArrayList &lineage)
{
    if (!node.isValid() || node.type().isEmpty())
        return;
    lineage.append(node.type());
    if (node.hasParentProperty())
        populateLineage(node.parentProperty().parentModelNode(), lineage);
}

}

namespace QmlDesigner {

std::vector<std::unique_ptr<Internal::NodeParserCreatorBase>> ComponentExporter::m_readers;
ComponentExporter::ComponentExporter(const ModelNode &rootNode):
    m_rootNode(rootNode)
{

}

QJsonObject ComponentExporter::exportComponent() const
{
    QTC_ASSERT(m_rootNode.isValid(), return {});
    return nodeToJson(m_rootNode);
}

ModelNodeParser *ComponentExporter::createNodeParser(const ModelNode &node) const
{
    QByteArrayList lineage;
    populateLineage(node, lineage);
    std::unique_ptr<ModelNodeParser> reader;
    for (auto &parserCreator: m_readers) {
        std::unique_ptr<ModelNodeParser> r(parserCreator->instance(lineage, node));
        if (r->isExportable()) {
            if (reader) {
                if (reader->priority() < r->priority())
                    reader = std::move(r);
            } else {
                reader = std::move(r);
            }
        }
    }
    if (!reader) {
        qCDebug(loggerInfo()) << "No parser for node" << node;
    }
    return reader.release();
}

QJsonObject ComponentExporter::nodeToJson(const ModelNode &node) const
{
    QJsonObject jsonObject;
    std::unique_ptr<ModelNodeParser> parser(createNodeParser(node));
    if (parser)
        jsonObject = parser->json();

    QJsonArray children;
    for (const ModelNode &childnode : node.directSubModelNodes())
        children.append(nodeToJson(childnode));

    if (!children.isEmpty())
        jsonObject.insert("children", children);

    return  jsonObject;
}


} // namespace QmlDesigner
