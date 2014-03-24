/* This file is part of the Springlobby (GPL v2 or later), see COPYING */
//
// Class: Channel
//

#include "channel.h"
#include "../ui.h"
#include "iserver.h"
#include "../user.h"
#include "../utils/tasutil.h"
#include "../utils/debug.h"
#include "../updater/updatehelper.h"
#include <wx/regex.h>
#include <wx/log.h>
#include "../chatpanel.h"
#include "utils/conversion.h"

Channel::Channel( IServer& serv )
    : m_serv(serv),
    m_do_ban_regex(false),
    m_do_unban_regex(false),
	m_userdata(NULL)
{}

Channel::~Channel() {
  if(uidata.panel)uidata.panel->SetChannel(NULL);
}

void Channel::SetName( const std::string& name )
{
  m_name = name;
}


std::string Channel::GetName() const
{
  return m_name;
}


User& Channel::GetMe()
{
  return m_serv.GetMe();
}


void Channel::Said( User& who, const std::string& message )
{
	wxLogDebugFunc( wxEmptyString );
	if (uidata.panel == 0 ) {
		wxLogError( _T("OnChannelSaid: ud->panel NULL") );
		return;
	}
	uidata.panel->Said( TowxString(who.GetNick()), TowxString(message));
}


void Channel::Say( const std::string& message )
{
  wxLogDebugFunc( wxEmptyString );
  m_serv.SayChannel( TowxString(m_name), TowxString(message));
}


void Channel::DidAction( User& who, const std::string& action )
{
	wxLogDebugFunc( wxEmptyString );
	if ( uidata.panel == 0 ) {
		wxLogError( _T("OnChannelDidAction: ud->panel NULL") );
		return;
	}
	uidata.panel->DidAction( TowxString(who.GetNick()), TowxString(action ));
}


void Channel::DoAction( const std::string& action )
{
	wxLogDebugFunc( wxEmptyString );
	m_serv.DoActionChannel( TowxString(m_name), TowxString(action) );
}


void Channel::Left( User& who, const std::string& reason )
{
	RemoveUser(who.GetNick());
	//wxLogDebugFunc( wxEmptyString );
	if ( uidata.panel == 0 ) {
		wxLogError( _T("OnUserLeftChannel: ud->panel NULL") );
		return;
	}
	uidata.panel->Parted( who, TowxString(reason));
}


void Channel::Leave()
{
	m_serv.PartChannel( TowxString(m_name) );
}

void Channel::Rejoin()
{
	m_serv.JoinChannel( TowxString(m_name), TowxString(m_password));
}


void Channel::Joined( User& who )
{
	AddUser( who );
	if ( uidata.panel == 0 ) {
		wxLogError( _T("OnUserJoinedChannel: ud->panel NULL") );
		return;
	}
	uidata.panel->Joined(who);
}


void Channel::OnChannelJoin( User& who )
{
	AddUser( who );
	//wxLogDebugFunc( wxEmptyString );
	if ( uidata.panel == 0 ) {
		wxLogError( _T("OnChannelJoin: ud->panel NULL") );
		return;
	}
	uidata.panel->OnChannelJoin( who );
}


void Channel::SetTopic( const std::string& topic, const std::string& who )
{
	m_topic = topic;
	m_topic_nick = who;

	wxLogDebugFunc( wxEmptyString );
	if ( uidata.panel == 0 ) {
		wxLogError( _T("OnChannelTopic: ud->panel NULL") );
		return;
	}
	uidata.panel->SetTopic( TowxString(who), TowxString(topic));
}

std::string Channel::GetTopicSetBy()
{
  return m_topic_nick;
}


std::string Channel::GetTopic()
{
  return m_topic;
}


void Channel::AddUser( User& user )
{
  UserList::AddUser( user );
  CheckBanned(user.GetNick());
}

void Channel::CheckBanned(const std::string& name){
  if(name=="ChanServ")return;
  if(m_banned_users.count(name)>0){
    m_serv.SayPrivate(_T("ChanServ"),_T("!kick #")+TowxString(GetName())+_T(" ")+TowxString(name));
  }
  if(m_do_ban_regex&&m_ban_regex.IsValid()){
    if(m_ban_regex.Matches(TowxString(name))&&!(m_do_unban_regex&&m_unban_regex.IsValid()&&m_unban_regex.Matches(TowxString(name)))){
      m_serv.SayPrivate(_T("ChanServ"),_T("!kick #")+TowxString(GetName())+_T(" ")+TowxString(name));
      if(!m_ban_regex_msg.empty())m_serv.SayPrivate(TowxString(name),TowxString(m_ban_regex_msg));
    }
  }
}

bool Channel::IsBanned(const std::string& name){
  if(name == "ChanServ")return false;
  if(m_banned_users.count(name)>0)return true;
  if(m_do_ban_regex&&m_ban_regex.IsValid()){
    if(m_ban_regex.Matches(TowxString(name))&&!(m_do_unban_regex&&m_unban_regex.IsValid()&&m_unban_regex.Matches(TowxString(name))))return true;
  }
  return false;
}


void Channel::RemoveUser( const std::string& nick )
{
  UserList::RemoveUser(nick);
}


bool Channel::ExecuteSayCommand( const std::string& in )
{
  if ( in.length() == 0 ) return true;

  if ( in[0] != '/' ) return false;

  wxString subcmd = TowxString(in).BeforeFirst(' ').Lower();
  wxString params = TowxString(in).AfterFirst( ' ' );

  wxString cmdline = TowxString(in);
  wxString param = GetWordParam( cmdline );
  if ( param == _T("/me") ) {
    DoAction(STD_STRING(cmdline));
    return true;
  } else if (( in == "/part") || (in == "/p") ) {
    Leave();
    uidata.panel = 0;
    return true;
  } else if ( param == _T("/sayver") ) {
	  //!this instance is not replaced with GetAppname for sake of help/debug online
    DoAction( "is using SpringLobby v" + GetSpringLobbyVersion() );
    return true;
  } else if(subcmd==_T("/userban")){
    m_banned_users.insert(STD_STRING(params));
    m_serv.SayPrivate(_T("ChanServ"),_T("!kick #")+TowxString(GetName())+_T(" ")+params);
    return true;
  } else if(subcmd==_T("/userunban")){
    m_banned_users.erase(STD_STRING(params));
    return true;
  } else if(subcmd==_T("/banregex")){
    ui().OnChannelMessage(*this,_T("/banregex ")+params);
    m_do_ban_regex=!params.empty();
    if(m_do_ban_regex){
      #ifdef wxHAS_REGEX_ADVANCED
      m_ban_regex.Compile(params, wxRE_ADVANCED);
      #else
      m_ban_regex.Compile(params, wxRE_EXTENDED);
      #endif
      if(!m_ban_regex.IsValid())ui().OnChannelMessage(*this,_T("Invalid regular expression"));
    }
    return true;
  } else if(subcmd==_T("/unbanregex")){
    ui().OnChannelMessage(*this,_T("/unbanregex ")+params);
    m_do_unban_regex=!params.empty();
    if(m_do_unban_regex){
      #ifdef wxHAS_REGEX_ADVANCED
      m_unban_regex.Compile(params, wxRE_ADVANCED);
      #else
      m_unban_regex.Compile(params, wxRE_EXTENDED);
      #endif
      if(!m_unban_regex.IsValid())ui().OnChannelMessage(*this,_T("Invalid regular expression"));
    }
    return true;
  } else if (subcmd==_T("/checkban")) {
    if(IsBanned(STD_STRING(params))){
      ui().OnChannelMessage(*this,params+_T(" is banned"));
    }else{
      ui().OnChannelMessage(*this,params+_T(" is not banned"));
    }
    return true;
  }


  else if(subcmd==_T("/banregexmsg")){
    ui().OnChannelMessage(*this,_T("/banregexmsg ")+params);
    m_ban_regex_msg=STD_STRING(params);
    return true;
  }

  return false;
}


std::string Channel::GetPassword()
{
  return m_password;
}


void Channel::SetPassword( const std::string& pw )
{
  m_password = pw;
}

