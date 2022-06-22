#include "writer.h"
#include "qdebug.h"
Writer::Writer(QObject *parent)
: QThread(parent)
{
}

void Writer::run()
{
}
void Writer::commandBrute(basicStruct *basic){
    //moveToThread(this->thread());
        //qDebug() << "passed" << data.toHex() << flagL;
    if(basic->m_flagClosePort==false){
        if(basic->flagL==awaitingResponse){
            emit (unleashPacketsSig(basic->pendingCommand));
            qDebug() << "stuck"  << basic->motherPacket;
        }else{
            if(basic->motherPacket.size()>0) {
                QByteArray rawData=basic->motherPacket.dequeue();
                emit (unleashPacketsSig(rawData));
                basic->pendingCommand = rawData;
                qDebug() << rawData.toHex();
                switch((quint8)rawData[2]){
                    case txSet:
                        basic->conditionPr=txSetDone;
                        basic->flagL=awaitingResponse;
                        //emit (setConditionPrSig(txSetDone));
                        //emit (setFlagLSig(awaitingResponse));
                        qDebug() << "txset";
                        break;
                    case txInc:
                        basic->conditionPr=txIncDone;
                        basic->flagL=awaitingResponse;
                        break;
                    case txDec:
                        basic->conditionPr=txDecDone;
                        basic->flagL=awaitingResponse;
                        break;
                    case hzSet:
                        basic->conditionPr=hzSetDone;
                        basic->flagL=awaitingResponse;
                        break;
                    case hzInc:
                        basic->conditionPr=hzIncDone;
                        basic->flagL=awaitingResponse;
                        break;
                    case hzDec:
                        basic->conditionPr=hzDecDone;
                        basic->flagL=awaitingResponse;
                        break;
                    case resModem:
                        basic->flagL=awaitingActivity;
                        break;
                }
            }else{
                qDebug() << "passed" << basic->flagL << basic->state << basic->motherPacket;
                    emit(initReqSig());
            }
        }
    }
}

void Writer::x(){
    qDebug() << "sdf";
}
