/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#pragma once

#include "projectexplorer_export.h"
#include "buildstep.h"

#include <utils/fileutils.h>
#include <utils/outputformat.h>

#include <functional>

namespace ProjectExplorer {
class Task;

// Documentation inside.
class PROJECTEXPLORER_EXPORT IOutputParser : public QObject
{
    Q_OBJECT
public:
    IOutputParser();
    ~IOutputParser() override;

    void handleStdout(const QString &data);
    void handleStderr(const QString &data);

    virtual bool hasFatalErrors() const;

    using Filter = std::function<QString(const QString &)>;
    void addFilter(const Filter &filter);

    void addSearchDir(const Utils::FilePath &dir);
    void dropSearchDir(const Utils::FilePath &dir);
    const Utils::FilePaths searchDirectories() const;

    void flush();
    void clear();

    void addLineParser(IOutputParser *parser);
    void addLineParsers(const QList<IOutputParser *> &parsers);
    void setLineParsers(const QList<IOutputParser *> &parsers);

#ifdef WITH_TESTS
    QList<IOutputParser *> lineParsers() const;
    void skipFileExistsCheck();
#endif

    void setRedirectionDetector(const IOutputParser *detector);

    static QString rightTrimmed(const QString &in);

signals:
    void searchDirIn(const Utils::FilePath &dir);
    void searchDirOut(const Utils::FilePath &dir);
    void addTask(const ProjectExplorer::Task &task, int linkedOutputLines = 0, int skipLines = 0);

protected:
    enum class Status { Done, InProgress, NotHandled };

    Utils::FilePath absoluteFilePath(const Utils::FilePath &filePath);

private:
    virtual Status doHandleLine(const QString &line, Utils::OutputFormat type);
    virtual void doFlush();
    virtual bool hasDetectedRedirection() const { return false; }

    void handleLine(const QString &line, Utils::OutputFormat type);
    QString filteredLine(const QString &line) const;
    void connectLineParser(IOutputParser *parser);
    bool needsRedirection() const;
    Utils::OutputFormat outputTypeForParser(const IOutputParser *parser,
                                            Utils::OutputFormat type) const;

    class OutputChannelState;
    class IOutputParserPrivate;
    IOutputParserPrivate * const d;
};

} // namespace ProjectExplorer
