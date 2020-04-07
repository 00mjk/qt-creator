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

#include "ioutputparser.h"
#include "task.h"

#include <utils/synchronousprocess.h>

/*!
    \class ProjectExplorer::IOutputParser

    \brief The IOutputParser class provides an interface for an output parser
    that emits issues (tasks).

    \sa ProjectExplorer::Task
*/

/*!
    \fn void ProjectExplorer::IOutputParser::appendOutputParser(IOutputParser *parser)

    Appends a subparser to this parser, of which IOutputParser will take
    ownership.
*/

/*!
    \fn IOutputParser *ProjectExplorer::IOutputParser::childParser() const

    Returns the head of this parser's output parser children. IOutputParser
    keeps ownership.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::stdOutput(const QString &line)

   Called once for each line if standard output to parse.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::stdError(const QString &line)

   Called once for each line if standard error to parse.
*/

/*!
   \fn bool ProjectExplorer::IOutputParser::hasFatalErrors() const

   This is mainly a Symbian specific quirk.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::addOutput(const QString &string, ProjectExplorer::BuildStep::OutputFormat format)

   Should be emitted whenever some additional information should be added to the
   output.

   \note This is additional information. There is no need to add each line.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::addTask(const ProjectExplorer::Task &task)

   Should be emitted for each task seen in the output.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::taskAdded(const ProjectExplorer::Task &task)

   Subparsers have their addTask signal connected to this slot.
   This function can be overwritten to change the task.
*/

/*!
   \fn void ProjectExplorer::IOutputParser::doFlush()

   Instructs a parser to flush its state.
   Parsers may have state (for example, because they need to aggregate several
   lines into one task). This
   function is called when this state needs to be flushed out to be visible.

   doFlush() is called by flush(). flush() is called on child parsers
   whenever a new task is added.
   It is also called once when all input has been parsed.
*/

namespace ProjectExplorer {

class OutputChannelState
{
public:
    using LineHandler = void (IOutputParser::*)(const QString &line);

    OutputChannelState(IOutputParser *parser, LineHandler lineHandler)
        : parser(parser), lineHandler(lineHandler) {}

    void handleData(const QString &newData)
    {
        pendingData += newData;
        pendingData = Utils::SynchronousProcess::normalizeNewlines(pendingData);
        while (true) {
            const int eolPos = pendingData.indexOf('\n');
            if (eolPos == -1)
                break;
            const QString line = pendingData.left(eolPos + 1);
            pendingData.remove(0, eolPos + 1);
            (parser->*lineHandler)(line);
        }
    }

    void flush()
    {
        if (!pendingData.isEmpty()) {
            (parser->*lineHandler)(pendingData);
            pendingData.clear();
        }
    }

    IOutputParser * const parser;
    LineHandler lineHandler;
    QString pendingData;
};

class IOutputParser::IOutputParserPrivate
{
public:
    IOutputParserPrivate(IOutputParser *parser)
        : stdoutState(parser, &IOutputParser::stdOutput),
          stderrState(parser, &IOutputParser::stdError)
    {}

    IOutputParser *childParser = nullptr;
    Utils::FilePath workingDir;
    OutputChannelState stdoutState;
    OutputChannelState stderrState;
};

IOutputParser::IOutputParser() : d(new IOutputParserPrivate(this))
{
}

IOutputParser::~IOutputParser()
{
    delete d->childParser;
    delete d;
}

void IOutputParser::handleStdout(const QString &data)
{
    d->stdoutState.handleData(data);
}

void IOutputParser::handleStderr(const QString &data)
{
    d->stderrState.handleData(data);
}

void IOutputParser::appendOutputParser(IOutputParser *parser)
{
    if (!parser)
        return;
    if (d->childParser) {
        d->childParser->appendOutputParser(parser);
        return;
    }

    d->childParser = parser;
    connect(parser, &IOutputParser::addTask, this, &IOutputParser::taskAdded);
}

IOutputParser *IOutputParser::childParser() const
{
    return d->childParser;
}

void IOutputParser::setChildParser(IOutputParser *parser)
{
    if (d->childParser != parser)
        delete d->childParser;
    d->childParser = parser;
    if (parser)
        connect(parser, &IOutputParser::addTask, this, &IOutputParser::taskAdded);
}

void IOutputParser::stdOutput(const QString &line)
{
    if (d->childParser)
        d->childParser->stdOutput(line);
}

void IOutputParser::stdError(const QString &line)
{
    if (d->childParser)
        d->childParser->stdError(line);
}

Utils::FilePath IOutputParser::workingDirectory() const { return d->workingDir; }

void IOutputParser::taskAdded(const Task &task, int linkedOutputLines, int skipLines)
{
    emit addTask(task, linkedOutputLines, skipLines);
}

void IOutputParser::doFlush()
{ }

bool IOutputParser::hasFatalErrors() const
{
    return d->childParser && d->childParser->hasFatalErrors();
}

void IOutputParser::setWorkingDirectory(const Utils::FilePath &fn)
{
    d->workingDir = fn;
    if (d->childParser)
        d->childParser->setWorkingDirectory(fn);
}

void IOutputParser::flush()
{
    flushTasks();
    d->stdoutState.flush();
    d->stderrState.flush();
    flushTasks();
}

void IOutputParser::flushTasks()
{
    doFlush();
    if (d->childParser)
        d->childParser->flushTasks();
}

QString IOutputParser::rightTrimmed(const QString &in)
{
    int pos = in.length();
    for (; pos > 0; --pos) {
        if (!in.at(pos - 1).isSpace())
            break;
    }
    return in.mid(0, pos);
}

} // namespace ProjectExplorer
