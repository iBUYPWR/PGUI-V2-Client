#include "portal.h"
#include <qdebug.h>
#include <QElapsedTimer>
#include "writer.h"
#include <QtConcurrent/QtConcurrent>
#include <QtConcurrent/QtConcurrentRun>
portal::portal(QObject *parent)
: QThread(parent)
{
    basic.state=voided;
    t_timer = new QTimer(0);
    t_timer->moveToThread(this->thread());
    t_timer->setInterval(300);
    connect(this, SIGNAL(writeSig(QByteArray)), this, SLOT(writeData(QByteArray)));
    //connect(parent, SIGNAL(commandBruteSig()), this, SLOT(commandBrute()));
    basic.flagL=initial;
    writer=new Writer();
    writer->start();
    connect(t_timer, SIGNAL(timeout()), this, SIGNAL(commandBruteSigInner()));
    connect(this, SIGNAL(commandBruteSigInner()), this, SLOT(regularCheck()));
    connect(this, SIGNAL(commandBruteSigOuter(basicStruct*)),writer,SLOT(commandBrute(basicStruct*)));
    connect(writer, SIGNAL(unleashPacketsSig(QByteArray)), this, SLOT(unleashPackets(QByteArray)));
    connect(writer, SIGNAL(initReqSig()), this, SLOT(initReq()));
    connect(writer, SIGNAL(setFlagLSig(Flag_State)), this, SLOT(setFlagL(Flag_State)));
    connect(writer, SIGNAL(setConditionPrSig(Prompt_Type)), this, SLOT(setConditionPr(Prompt_Type)));
    t_timer->start();
}
void portal::run()
{
    Loop_State loopState = idleAck;
    QByteArray compiledData;
    int bytesToProcess=0;
    while(!done){
        //qDebug() << "Doing regular check pass 1" ;
        if(response_timer.elapsed()>=1500 && general_timer.elapsed()>=1500){
            ///qDebug() << general_timer.elapsed();
            emit packetStatusSig(false,"No response from the Radio and the connection seems void.");
            basic.state=voided;
            basic.flagL=awaitingActivity;
        } else if (response_timer.elapsed()>=1500 && general_timer.elapsed()<=1500){
            emit packetStatusSig(false,"Packets are detected on line, but no response from the radio");
            basic.flagL=awaitingActivity;
        }
        if(m_receiveBufferMutex.tryLock()){
            if(m_recieveBufferQueue.size() > 0){
            data = m_recieveBufferQueue.dequeue();
            m_receiveBufferMutex.unlock();
            //int length = data.length();
            //QByteArray noCheckSum = data.left(length-2);
            for (int i = 0; i < data.size(); i++){
            //if((quint8)data[0]==firstChk && (quint8)data[1]==secondChk){
                //qDebug() << "signature true" ;
                switch (loopState) {
                    case idleAck:
                        bytesToProcess=0;
                        compiledData.clear();
                        if((quint8)data[i]==firstChk){
                            loopState=sigAck1;
                            compiledData.append(data[i]);
                        }else{
                            loopState=idleAck;
                        }
                        break;
                    case sigAck1:
                        if((quint8)data[i]==secondChk){
                            loopState=sigAck2;
                            compiledData.append(data[i]);
                        }else{
                            loopState=idleAck;
                        }
                        break;
                    case sigAck2:
                        compiledData.append(data[i]);
                        switch ((quint8)data[i]) {
                            case hzSet:
                                bytesToProcess=7;
                                basic.lastPr=hzSetDone;
                                loopState=commandAck;
                                break;
                            case hzInc:
                                bytesToProcess=7;
                                basic.lastPr=hzIncDone;
                                loopState=commandAck;
                                break;
                            case hzDec:
                                bytesToProcess=7;
                                basic.lastPr=hzDecDone;
                                loopState=commandAck;
                                break;
                            case txSet:
                                bytesToProcess=6;
                                basic.lastPr=txSetDone;
                                loopState=commandAck;
                                break;
                            case txDec:
                                bytesToProcess=6;
                                basic.lastPr=txDecDone;
                                loopState=commandAck;
                                break;
                            case txInc:
                                bytesToProcess=6;
                                basic.lastPr=txSetDone;
                                loopState=commandAck;
                                break;
                            case reqStatus:
                                bytesToProcess=22;
                                basic.lastPr=wDone;
                                loopState=commandAck;
                                response_timer.restart();
                                emit packetStatusSig(true,"Connection is healthy.");
                                break;
                            default:
                                loopState=idleAck;
                                break;
                        }
                        if((quint8)data[i]!=basic.conditionPr)basic.flagL=initial;
                        break;
                    case commandAck:
                        if(bytesToProcess - 5 > 0){
                            compiledData.append(data[i]);
                            bytesToProcess--;
                            loopState=commandAck;
                        }else{
                            i--; //To reprocess the same value
                            loopState=dataAck;
                        }
                        break;
                    case dataAck:
                        if(((makeCheckSum(compiledData)>> 8)& 0xFF)==(quint8)data[i]){
                            loopState=msbAck;
                        }else{
                            loopState=idleAck;
                            i--; //To reprocess the same value
                        }
                        break;
                    case msbAck:
                        if((makeCheckSum(compiledData) & 0xFF)==(quint8)data[i]){
                            loopState=idleAck;
                            basic.flagL=initial;
                            responseCounter=0;
                            emit syncCallSig(compiledData);
                        }else{
                            loopState=idleAck;
                            i--; //To reprocess the same value
                        }
                        break;
                    case lsbAck:
                        loopState=idleAck;
                        break;
                    }
                }
                qDebug() << "loop" << loopState << "flag" << basic.flagL;
                if(loopState!=idleAck)basic.flagL=awaitingCompletion; // States left midway
            }else{
                m_receiveBufferMutex.unlock();
                msleep(1);
            }
         }else{
            msleep(5);
        }
    }
}

void portal::openPort(QString setting,int baudrate,QString parity){
    m_serialPort = new QSerialPort();
    m_serialPort->setPortName(setting);
    m_serialPort->setBaudRate(baudrate);
    //m_serialPort->setDataBits(QSerialPort::Data8);
    //m_serialPort->setStopBits(QSerialPort::OneStop);
    //m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    if (parity == "None"){
        m_serialPort->setParity(QSerialPort::NoParity);
    }
    else if (parity == "Odd"){
        m_serialPort->setParity(QSerialPort::OddParity);
    }
    try
    {
        if (m_serialPort->open(QIODevice::ReadWrite))
        {
            emit connectionStatusSig(true);
            emit packetStatusSig(false,"Connection is void.");
            qDebug() << "open.";
            basic.m_flagClosePort = false;
            emit devoidSig();
            done = false;
            connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
            connect(m_serialPort,&QSerialPort::bytesWritten, this, [](const qint64 bytes) {
                qDebug() << "Sent data: " << bytes << " bytes.";
            });
            //t_timer->sta1rt(1000);
            general_timer.start();
            response_timer.start();
            if (!isRunning())
            {
                start();
            }
        }
        else
        {
            done = true;
            basic.m_flagClosePort = true;
            emit connectionStatusSig(false);
        }
    }
    catch (...)
    {

    }
}
void portal::closePort()
{

    if (m_serialPort->isOpen()){
        m_serialPort->close();
	}
    done = true;
    basic.m_flagClosePort = false;
    emit connectionStatusSig(false);
}
void portal::qwe(){
    qDebug() << "sd0";
}
void portal::handleReadyRead(){
    QByteArray handleData=m_serialPort->readAll();
    qDebug() << "data:" << handleData.toHex() << handleData.size();
    //qDebug() << silence_timer.elapsed();
    general_timer.restart();
    silence_timer.restart();
    m_receiveBufferMutex.lock();
    m_recieveBufferQueue.enqueue(handleData);
    m_receiveBufferMutex.unlock();
    if(basic.state==voided){
        basic.state=silence;
    }

}

void portal::regularCheck(){
    qDebug() << "Doing regular check" <<general_timer.elapsed()<< response_timer.elapsed() << basic.flagL ;
    emit commandBruteSigOuter(&basic);
    basic.mode=waiting;
    //extern void commandBrute();
    //QFuture<void> f = QtConcurrent::run(commandBrute);

}
void portal::writeData(QByteArray data)
{
    //qint64 bytesWritten =
    m_serialPort->write(data);

}
int portal::makeCheckSum(QByteArray data){
	int sum = 0;
	
	for (int i = 0; i < data.count(); i++)
	{
		sum += quint8(data[i]);
	}
	return sum;
}
//void portal::commandBrute(){

void portal::unleashPackets(QByteArray body){
    emit writeSig(body);
    qDebug() << "written" << body.toHex();
}
void portal::initReq(){
    basic.lastPr=wRes;
    //send a simple request, to fetch status
    QByteArray data;
    data.append(firstChk);
    data.append(secondChk);
    data.append(reqStatus);
    int checkSum=makeCheckSum(data);
    data.append((checkSum >> 8)& 0xFF);
    data.append(checkSum & 0xFF);
    ///qDebug() << "initReq" << data.toHex();
    emit writeSig(data);
}
void portal::bodyReq(QByteArray body){
    int checkSum=makeCheckSum(body);
    body.append((checkSum >> 8)& 0xFF);
    body.append(checkSum & 0xFF);
    basic.motherPacket.enqueue(body);
}
void portal::setFlagL(Flag_State tmp){
    basic.flagL = tmp;
}
void portal::setConditionPr(Prompt_Type tmp){
    basic.conditionPr=tmp;
}
