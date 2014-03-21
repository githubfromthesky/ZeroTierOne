/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "Peer.hpp"
#include "Switch.hpp"

#include <algorithm>

namespace ZeroTier {

Peer::Peer() :
	_id(),
	_lastUsed(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_lastPinged(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_latency(0)
{
}

Peer::Peer(const Identity &myIdentity,const Identity &peerIdentity)
	throw(std::runtime_error) :
	_id(peerIdentity),
	_lastUsed(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_lastPinged(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_latency(0)
{
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::onReceive(
	const RuntimeEnvironment *_r,
	const InetAddress &remoteAddr,
	unsigned int hops,
	uint64_t packetId,
	Packet::Verb verb,
	uint64_t inRePacketId,
	Packet::Verb inReVerb,
	uint64_t now)
{
	if (!hops) { // direct packet
		// Update last receive info for our direct path
		WanPath *const wp = (remoteAddr.isV4() ? &_ipv4p : &_ipv6p);
		wp->lastReceive = now;
		if (!wp->fixed)
			wp->addr = remoteAddr;

		// Announce multicast LIKEs to peers to whom we have a direct link
		if ((now - _lastAnnouncedTo) >= ((ZT_MULTICAST_LIKE_EXPIRE / 2) - 1000)) {
			_lastAnnouncedTo = now;
			_r->sw->announceMulticastGroups(SharedPtr<Peer>(this));
		}
	}

	if (verb == Packet::VERB_FRAME) {
		_lastUnicastFrame = now;
	} else if (verb == Packet::VERB_MULTICAST_FRAME) {
		_lastMulticastFrame = now;
	}
}

bool Peer::send(const RuntimeEnvironment *_r,const void *data,unsigned int len,uint64_t now)
{
	if ((_ipv6p.isActive(now))||((!(_ipv4p.addr))&&(_ipv6p.addr))) {
		if (_r->sm->send(_ipv6p.addr,false,data,len)) {
			_ipv6p.lastSend = now;
			return true;
		}
	}

	if (_ipv4p.addr) {
		if (_r->sm->send(_ipv4p.addr,false,data,len)) {
			_ipv4p.lastSend = now;
			return true;
		}
	}

	return false;
}

bool Peer::sendFirewallOpener(const RuntimeEnvironment *_r,uint64_t now)
{
	bool sent = false;
	if (_ipv4p.addr) {
		if (_r->sm->sendFirewallOpener(_ipv4p.addr,ZT_FIREWALL_OPENER_HOPS)) {
			_ipv4p.lastFirewallOpener = now;
			sent = true;
		}
	}

	if (_ipv6p.addr) {
		if (_r->sm->sendFirewallOpener(_ipv6p.addr,ZT_FIREWALL_OPENER_HOPS)) {
			_ipv6p.lastFirewallOpener = now;
			sent = true;
		}
	}

	return sent;
}

bool Peer::sendPing(const RuntimeEnvironment *_r,uint64_t now)
{
	bool sent = false;
	if (_ipv4p.addr) {
		TRACE("PING %s(%s)",_id.address().toString().c_str(),_ipv4p.addr.toString().c_str());
		if (_r->sw->sendHELLO(SharedPtr<Peer>(this),_ipv4p.addr,false)) {
			_ipv4p.lastSend = now;
			sent = true;
		}
	}

	if (_ipv6p.addr) {
		TRACE("PING %s(%s)",_id.address().toString().c_str(),_ipv6p.addr.toString().c_str());
		if (_r->sw->sendHELLO(SharedPtr<Peer>(this),_ipv6p.addr,false)) {
			_ipv6p.lastSend = now;
			sent = true;
		}
	}

	return sent;
}

void Peer::setPathAddress(const InetAddress &addr,bool fixed)
{
	if (addr.isV4()) {
		_ipv4p.addr = addr;
		_ipv4p.fixed = fixed;
	} else if (addr.isV6()) {
		_ipv6p.addr = addr;
		_ipv6p.fixed = fixed;
	}
}

void Peer::clearFixedFlag(InetAddress::AddressType t)
{
	switch(t) {
		case InetAddress::TYPE_NULL:
			_ipv4p.fixed = false;
			_ipv6p.fixed = false;
			break;
		case InetAddress::TYPE_IPV4:
			_ipv4p.fixed = false;
			break;
		case InetAddress::TYPE_IPV6:
			_ipv6p.fixed = false;
			break;
	}
}

} // namespace ZeroTier
