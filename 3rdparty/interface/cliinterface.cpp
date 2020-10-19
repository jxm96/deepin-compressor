/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     gaoxiang <gaoxiang@uniontech.com>
*
* Maintainer: gaoxiang <gaoxiang@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "cliinterface.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>

CliInterface::CliInterface(QObject *parent, const QVariantList &args)
    : ReadWriteArchiveInterface(parent, args)
{
    setWaitForFinishedSignal(true);
    if (QMetaType::type("QProcess::ExitStatus") == 0) {
        qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    }

    m_cliProps = new CliProperties(this, m_metaData, m_mimetype);
}

CliInterface::~CliInterface()
{

}

bool CliInterface::list()
{
    return true;
}

bool CliInterface::testArchive()
{
    return true;
}

bool CliInterface::extractFiles(const QVector<FileEntry> &files, const ExtractionOptions &options)
{
    return true;
}

bool CliInterface::addFiles(const QVector<FileEntry> &files, const CompressOptions &options)
{
    m_workStatus = WT_Add;

    bool ret = false;
    QFileInfo fi(m_strArchiveName);  // 压缩文件（全路径）
    QVector<FileEntry> tempfiles = files;
    QStringList fileList;
    // 获取待压缩的文件
    for (int i = 0; i < tempfiles.size(); i++) {
        fileList.append(tempfiles.at(i).strFullPath);
    }

    // 压缩命令的参数
    QStringList arguments = m_cliProps->addArgs(m_strArchiveName,
                                                fileList,
                                                options.strPassword,
                                                options.bHeaderEncryption,
                                                options.iCompressionLevel,
                                                options.strCompressionMethod,
                                                options.strEncryptionMethod,
                                                options.iVolumeSize,
                                                options.bTar_7z,
                                                fi.path());

    ret = runProcess(m_cliProps->property("addProgram").toString(), arguments);
    return ret;
}

bool CliInterface::moveFiles(const QVector<FileEntry> &files, const CompressOptions &options)
{
    return true;
}

bool CliInterface::copyFiles(const QVector<FileEntry> &files, const CompressOptions &options)
{
    return true;
}

bool CliInterface::deleteFiles(const QVector<FileEntry> &files)
{
    return true;
}

bool CliInterface::addComment(const QString &comment)
{
    return true;
}

bool CliInterface::runProcess(const QString &programName, const QStringList &arguments)
{
    Q_ASSERT(!m_process);

    QString programPath = QStandardPaths::findExecutable(programName);
    if (programPath.isEmpty()) {
        return false;
    }

    m_process = new KProcess;
    m_process->setOutputChannelMode(KProcess::MergedChannels);
    m_process->setNextOpenMode(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::Text);
    m_process->setProgram(programPath, arguments);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [ = ]() {
        readStdout();
    });


    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CliInterface::processFinished);

    m_stdOutData.clear();
    m_process->start();

    return true;
}

void CliInterface::deleteProcess()
{
    if (m_process) {
        readStdout(true);

        delete m_process;
        m_process = nullptr;
    }
}

void CliInterface::killProcess(bool emitFinished)
{
    Q_UNUSED(emitFinished);
    if (!m_process) {
        return;
    }

    m_process->kill();
}

void CliInterface::readStdout(bool handleAll)
{
    Q_ASSERT(m_process);

    if (!m_process->bytesAvailable()) {
        // if process has no more data, we can just bail out
        return;
    }

    QByteArray dd = m_process->readAllStandardOutput();
    m_stdOutData += dd;
    QList<QByteArray> lines = m_stdOutData.split('\n');

//    bool wrongPasswordMessage = isWrongPasswordMsg(QLatin1String(lines.last()));

    if (m_process->program().length() > 2) {
        if ((m_process->program().at(0).contains("7z") && m_process->program().at(1) != "l")/* && !wrongPasswordMessage*/) {
            handleAll = true; // 7z output has no \n
        }
    }

    // this is complex, here's an explanation:
    // if there is no newline, then there is no guaranteed full line to
    // handle in the output. The exception is that it is supposed to handle
    // all the data, OR if there's been an error message found in the
    // partial data.
//    if (lines.size() == 1 && !handleAll) {
//        return;
//    }

    if (handleAll) {
        m_stdOutData.clear();
    } else {
        // because the last line might be incomplete we leave it for now
        // note, this last line may be an empty string if the stdoutdata ends
        // with a newline
        m_stdOutData = lines.takeLast();
    }

    for (const QByteArray &line : qAsConst(lines)) {
        if (!line.isEmpty()) {
            if (!handleLine(QString::fromLocal8Bit(line), m_workStatus)) {
                killProcess();
                return;
            }
        }
    }
}

void CliInterface::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_exitCode = exitCode;
    qDebug() << "Process finished, exitcode:" << exitCode << "exitstatus:" << exitStatus;

    deleteProcess();

    emit signalprogress(1.0);
    emit signalFinished(true);
}
