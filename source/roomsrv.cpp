#include "roomsrv.h"

RoomSrv::RoomSrv(QObject *parent, int _num, int _id)
    : QObject(parent),rt(this,_num,_id)
{
    connect(&rt,&RuntimeWrapper::emitMessage,this,&RoomSrv::processRuntimeMessage);
    connect(&rt,&RuntimeWrapper::finished,&rt,&RuntimeWrapper::deleteLater);
    num=_num;
    ready=0;
    id=_id;
    allowJoin=true;
    allowExit=true;
}
void RoomSrv::returnResult(Message msg, bool res){
    Message response(msg.getType(),msg.getSubtype(),msg.getSenderType(),msg.getSenderid(),msg.getReceiverType(),msg.getReceiverid());
    response.addArgument(res);
    emit emitMessage(res);
}
bool RoomSrv::event(QEvent *e){
    if(e->type()!=(QEvent::Type)2333)
        return QObject::event(e);
    Message tmp=*(Message *)e;
    if(tmp.getType()==1){
        if(map.key(tmp.getSenderid(),-1)==-1)
            return false;
        tmp.setSenderid(map.key(tmp.getSenderid()));
        return rt.processMessage(tmp);
    }
    else if(tmp.getType()==2){
        switch(tmp.getSubtype()){
        case 0:
            returnResult(tmp,addPlayer(tmp.getSenderid()));
            break;
        case 1:
            ready++;
            sendRoomInfo(-1);
            if(ready==num)
                startGame();
            break;
        case 2:
            if(!allowExit)
                returnResult(tmp,false);
            else{
                ready--;
                returnResult(tmp,true);
                sendRoomInfo(-1);
            }
            break;
        case 3:
            if(tmp.getArgument().isEmpty())
                return false;
            returnResult(tmp,removePlayer(tmp.getArgument()[0],tmp.getSenderid()));
            break;
        case 6:
            sendRoomInfo(tmp.getSenderid());
            break;
        case 7:
            if(tmp.getArgument().isEmpty())
                return false;
            if(inDiscussion&&tmp.getArgument()[0]==0){
                tmp.setReceiverType(1);
                tmp.setReceiverid(-2);
                QVector<int> arg;
                arg.append(tmp.getSenderid());
                tmp.setArgument(arg);
                redirectMessage(tmp);
            }
            else if(tmp.getArgument()[0]==1){
                speakerCount--;
                if(inGame)
                    rt.stopWaitForPlayer(tmp.getSenderid());
            }
            else if(tmp.getArgument()[0]==-1){
                speakerCount=0;
                if(inGame)
                    rt.stopWaitForPlayer(-1);
            }
            if(speakerCount==0)
                inDiscussion=false;
        }
    }
    return false;
}
void RoomSrv::processRuntimeMessage(Message msg){
    redirectMessage(msg);
    QVector<int> arg=msg.getArgument();
    if(msg.getType()==1){
        if(msg.getSubtype()==5){
            QVector<int> speaker=msg.getArgument();
            openDiscussion(0,&speaker);
        }
        else if(msg.getSubtype()==12)
            openDiscussion();
        else if(msg.getSubtype()==19)
            if(msg.getArgument()[0]){
                if(inDiscussion)
                    speakerCount=0;
            }
    }
    else if(msg.getType()==2){
        if(msg.getSubtype()==5){
            inGame=false;
            allowExit=true;
            QVector<int> players=map.keys().toVector();
            openDiscussion(0,&players);
        }
    }
}
bool RoomSrv::addPlayer(int id){
    if(!allowJoin)
        return false;
    int i=0;
    while(map.contains(i))i++;
    map.insert(i,id);
    if(map.size()==num)
        allowJoin=false;
    Message msg(2,0,1,id,2,this->id);
    msg.addArgument(1);
    emit emitMessage(msg);
    sendRoomInfo(-1);
    qDebug()<<"player "<<id<<" added";
    return true;
}
bool RoomSrv::removePlayer(bool force, int id){
    if(!allowExit&&!force)
        return false;
    Message tmp(2,8);
    tmp.addArgument(0);
    emit emitMessage(tmp);
    map.remove(map.key(id));
    if(inGame)
        rt.playerOffline(id);
    if(map.isEmpty()){
        allowJoin=false;
        this->deleteLater();
    }
    else if(!inGame)
        allowJoin=true;
    sendRoomInfo(-1);
    return true;
}
void RoomSrv::startGame(){
    allowExit=false;
    allowJoin=false;
    inGame=true;
    Message tmp(2,4);
    emit emitMessage(tmp);
    tmp.setReceiverType(1);
    tmp.setReceiverid(-1);
    redirectMessage(tmp);
    rt.start();
}
void RoomSrv::redirectMessage(Message msg){
    msg.setSenderType(2);
    msg.setSenderid(id);
    if(msg.getReceiverType()==1){
        if(msg.getReceiverid()==-1)
            for(int i=0;i<num;i++)
                if(map.contains(i)){
                    msg.setReceiverid(map[i]);
                    emit emitMessage(msg);
                }

        else if(msg.getReceiverid()==-2)
            for(int i=0;i<audience.size();i++){
                msg.setReceiverid(map[audience[i]]);
                emit emitMessage(msg);
            }
            else{
                    msg.setReceiverid(map[msg.getReceiverid()]);
                    emit emitMessage(msg);
                }

    }
    else
        emit emitMessage(msg);
}
void RoomSrv::sendRoomInfo(int receiver){
    qDebug()<<"In sendRoomInfo..";
    Message info(2,6,1,receiver);
    info.addArgument(num);
    info.addArgument(map.size());
    info.addArgument(ready);
    for(int i=0;i<num;i++)
        if(map.contains(i)){
            info.addArgument(i);
            info.addArgument(map[i]);
        }
    redirectMessage(info);
    info.setReceiverType(0);
    redirectMessage(info);
    qDebug()<<"Out sendRoomInfo...";
}
void RoomSrv::openDiscussion(int receiver, QVector<int> *list){
    if(receiver==-1){
        speakerCount=1;
        audience.clear();
        for(int i=0;i<num;i++)
            if(map.contains(i))
                audience.append(i);
    }
    else{
        speakerCount=list->size();
        audience=*list;
    }
    inDiscussion=true;
}
int RoomSrv::getID(){
    return id;
}
