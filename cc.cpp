// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include <math.h>
#include <iostream>
#include <algorithm>
#include "cc.h"
#include "queue.h"
#include <stdio.h>
#include "switch.h"
#include "ecn.h"
using namespace std;

////////////////////////////////////////////////////////////////
//  CC SOURCE. Aici este codul care ruleaza pe transmitatori. Tot ce avem nevoie pentru a implementa
//  un algoritm de congestion control se gaseste aici.
////////////////////////////////////////////////////////////////
int CCSrc::_global_node_count = 0;

CCSrc::CCSrc(EventList &eventlist)
    : EventSource(eventlist,"cc"), _flow(NULL)
{
    _mss = Packet::data_packet_size();
    _acks_received = 0;
    _nacks_received = 0;

    _highest_sent = 0;
    _next_decision = 0;
    _flow_started = false;
    _sink = 0;
  
    _node_num = _global_node_count++;
    _nodename = "CCsrc " + to_string(_node_num);

    _cwnd = 10 * _mss;
    _ssthresh = 0xFFFFFFFFFF;
    _flightsize = 0;

    _W_last_max = 0;
    _epoch_start = 0;
    _origin_point = 0;
    _d_Min = 0;
    _W_tcp = 0;
    _K = 0;
    _ack_cnt = 0;
    _tcp_friendliness = 1;
    _fast_convergence = 1;
    _beta = 0.2;
    _C = 0.4; 

    _cwnd_cnt = 0;
    _cnt = 0;
    _max_cnt = 0;

    _flow._name = _nodename;
    setName(_nodename);
}

/* Porneste transmisia fluxului de octeti */
void CCSrc::startflow(){
    cout << "Start flow " << _flow._name << " at " << timeAsSec(eventlist().now()) << "s" << endl;
    _flow_started = true;
    _highest_sent = 0;
    _packets_sent = 0;

    while (_flightsize + _mss < _cwnd)
        send_packet();
}

/* Initializeaza conexiunea la host-ul sink */
void CCSrc::connect(Route* routeout, Route* routeback, CCSink& sink, simtime_picosec starttime) {
    assert(routeout);
    _route = routeout;
    
    _sink = &sink;
    _flow._name = _name;
    _sink->connect(*this, routeback);

    eventlist().sourceIsPending(*this,starttime);
}


/* Variabilele cu care vom lucra:
    _nacks_received
    _flightsize -> numarul de bytes aflati in zbor
    _mss -> maximum segment size
    _next_decision 
    _highest_sent
    _cwnd
    _ssthresh
    
    CCAck._ts -> timestamp ACK
    eventlist.now -> timpul actual
    eventlist.now - CCAck._tx -> latency
    
    ack.ackno();
    
    > Puteti include orice alte variabile in restul codului in functie de nevoie.
*/
/* TODO: In mare parte aici vom avea implementarea algoritmului si in functie de nevoie in celelalte functii */


//Aceasta functie este apelata atunci cand dimensiunea cozii a fost depasita iar packetul cu numarul de secventa ackno a fost aruncat.
void CCSrc::processNack(const CCNack& nack){    
    //cout << "CC " << _name << " got NACK " <<  nack.ackno() << _highest_sent << " at " << timeAsMs(eventlist().now()) << " us" << endl;    
    _nacks_received ++;    
    _flightsize -= _mss;    

    
    if (nack.ackno() >= _next_decision) {
        _epoch_start = 0;
        if (_cwnd <  _W_last_max && _fast_convergence) {
            _W_last_max = _cwnd * (2 - _beta) / 2.0;
        } else {
            _W_last_max = _cwnd;
            
        }
        _ssthresh = _cwnd = _cwnd * (1 - _beta);
        _next_decision = _highest_sent + _cwnd;
    }
}
    
/* Process an ACK.  Mostly just housekeeping*/    
void CCSrc::processAck(const CCAck& ack) {    
    CCAck::seq_t ackno = ack.ackno();    
    _acks_received++;
    _flightsize -= _mss;  

    simtime_picosec ack_timestamp = ack.ts();
    simtime_picosec now = eventlist().now();
    double rtt = timeAsMs(2 * (now - ack_timestamp));
    if (_d_Min) {
        _d_Min = min(_d_Min, rtt);
    } else {
        _d_Min = rtt;
    }

    if (_cwnd <= _ssthresh) {
        _cwnd += _mss;
    } else {
        cubic_update();
        if (_cwnd_cnt > _cnt) {
            _cwnd += _mss;
            _cwnd_cnt = 0;
        }  else {
            _cwnd_cnt += 1;
        }
    }
    
    //cout << "CWNDI " << _cwnd/_mss << endl;    
}    

void CCSrc::cubic_update() {
    _ack_cnt++;
    if (_epoch_start <= 0) {
        _epoch_start = eventlist().now();
        if (_cwnd < _W_last_max) {
            _K = cbrt((_W_last_max - _cwnd)/ _C);
            _origin_point = _W_last_max;
        } else {
            _K = 0;
            _origin_point = _cwnd;
        }
        _ack_cnt = 1;
        _W_tcp = _cwnd;
    }
    simtime_picosec now = eventlist().now();
    double t = timeAsMs(now) + _d_Min - timeAsMs(_epoch_start);
    double target =  _origin_point + _C * (t - _K) * (t - _K) * (t - _K);
    if (target > _cwnd) {
        _cnt = _cwnd / (target - _cwnd);
    } else {
        _cnt = 100 * _cwnd;
    }
    if (_tcp_friendliness) {
        cubic_friendliness();
    }

}

void CCSrc::cubic_friendliness() { 
    _W_tcp = _W_tcp + (3 * _beta) / (2 - _beta) * _ack_cnt / _cwnd;
    _ack_cnt = 0;
    if (_W_tcp > _cwnd) {
        _max_cnt = _cwnd / (_W_tcp - _cwnd);
        if (_cnt > _max_cnt) {
            _cnt = _max_cnt;
        }
    }
}

void CCSrc::cubic_reset() {
    _W_last_max = 0;
    _epoch_start = 0;
    _origin_point = 0;
    _d_Min = 0;
    _W_tcp = 0;
    _K = 0;
    _ack_cnt = 0;

}

void CCSrc::timeout() {
    cubic_reset();
}
/* Functia de receptie, in functie de ce primeste cheama processLoss sau processACK */
void CCSrc::receivePacket(Packet& pkt) 
{
    if (!_flow_started){
        return; 
    }

    switch (pkt.type()) {
    case CCNACK: 
        processNack((const CCNack&)pkt);
        pkt.free();
        break;
    case CCACK:
        processAck((const CCAck&)pkt);
        pkt.free();
        break;
    default:
        cout << "Got packet with type " << pkt.type() << endl;
        abort();
    }

    //now send packets!
    while (_flightsize + _mss < _cwnd)
        send_packet();
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
/* Functia care se este chemata pentru transmisia unui pachet */
void CCSrc::send_packet() {
    CCPacket* p = NULL;

    assert(_flow_started);

    p = CCPacket::newpkt(*_route,_flow, _highest_sent+1, _mss, eventlist().now());
    
    _highest_sent += _mss;
    _packets_sent++;

    _flightsize += _mss;

    //cout << "Sent " << _highest_sent+1 << " Flow Size: " << _flow_size << " Flow " << _name << " time " << timeAsUs(eventlist().now()) << endl;
    p->sendOn();
}

void CCSrc::doNextEvent() {
    if (!_flow_started){
      startflow();
      return;
    }
}

////////////////////////////////////////////////////////////////
//  CC SINK Aici este codul ce ruleaza pe receptor, in mare nu o sa aducem multe modificari
////////////////////////////////////////////////////////////////

/* Only use this constructor when there is only one for to this receiver */
CCSink::CCSink()
    : Logged("CCSINK"), _total_received(0) 
{
    _src = 0;
    
    _nodename = "CCsink";
    _total_received = 0;
}

/* Connect a src to this sink. */ 
void CCSink::connect(CCSrc& src, Route* route)
{
    _src = &src;
    _route = route;
    setName(_src->_nodename);
}


// Receive a packet.
// seqno is the first byte of the new packet.
void CCSink::receivePacket(Packet& pkt) {
    switch (pkt.type()) {
    case CC:
        break;
    default:
        abort();
    }

    CCPacket *p = (CCPacket*)(&pkt);
    CCPacket::seq_t seqno = p->seqno();

    simtime_picosec ts = p->ts();
    //bool last_packet = ((CCPacket*)&pkt)->last_packet();

    if (pkt.header_only()){
        send_nack(ts,seqno);      
    
        p->free();

        //cout << "Wrong seqno received at CC SINK " << seqno << " expecting " << _cumulative_ack << endl;
        return;
    }

    int size = p->size()-ACKSIZE; 
    _total_received += Packet::data_packet_size();;

    bool ecn = (bool)(pkt.flags() & ECN_CE);

    send_ack(ts,seqno,ecn);
    // have we seen everything yet?
    pkt.free();
}

void CCSink::send_ack(simtime_picosec ts,CCPacket::seq_t ackno,bool ecn) {
    CCAck *ack = 0;
    ack = CCAck::newpkt(_src->_flow, *_route, ackno,ts,ecn);
    ack->sendOn();
}

void CCSink::send_nack(simtime_picosec ts, CCPacket::seq_t ackno) {
    CCNack *nack = NULL;
    nack = CCNack::newpkt(_src->_flow, *_route, ackno,ts);
    nack->sendOn();
}
