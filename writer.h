#ifndef WRITER_H
#define WRITER_H
#include "QThread"
#include "QObject"
#include "newtype.h"
#include "qqueue.h"
#include "qtimer.h"
class Writer: public QThread
{
    Q_OBJECT
public:
    struct basicStruct{
        State_Type	mode;
        State_Type	state;
        Prompt_Type	lastPr;
        Prompt_Type	conditionPr;
        Flag_State 	flagL;
        Prompt_Waiting_Type waitingResponse;
        QQueue<QByteArray> motherPacket;
        QByteArray pendingCommand;
        bool	m_flagClosePort;
    };
    Writer(QObject *parent = nullptr);
    void run() Q_DECL_OVERRIDE;
    QTimer *t_timer;
public slots:
    void x();
    void commandBrute(basicStruct *basic);
signals:
    void initReqSig();
    void unleashPacketsSig(QByteArray);
    void setFlagLSig(Flag_State);
    void setConditionPrSig(Prompt_Type);
    void ex();
};
#endif // WRITER_H
