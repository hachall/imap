#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include "imap.hpp"
using namespace std;
using namespace IMAP;

/*SESSION CLASS MEMBER FUNCTION DEFINITIONS*/
Session::Session(std::function<void()> updateUI) {
  this->updateUI = updateUI;
  this->mailbox = "";
  this->imap_session = mailimap_new(0, NULL);
};

void Session::connect(string const& server, size_t port) {
  check_error(mailimap_socket_connect(this->imap_session, server.c_str(), port), "couldn't connect to server");
};

void Session::login(string const& userid, string const& password) {
  check_error(mailimap_login(this->imap_session, userid.c_str(), password.c_str()), "couldn't log in");
};

void Session::selectMailbox(string const& mailbox) {
  check_error(mailimap_select(this->imap_session, mailbox.c_str()), "mailbox couldn't be selected");
  this->mailbox = mailbox;
};

uint32_t Session::getUID(struct mailimap_msg_att* msg_att) {
  for (clistiter* cur = clist_begin(msg_att->att_list) ; cur != NULL ; cur = clist_next(cur)) {
    //static cast of clist void* to mailimap_msg_att_item* type
    mailimap_msg_att_item* item = (mailimap_msg_att_item*) clist_content(cur);

    if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC)
      continue;
    if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_UID)
      continue;

    return item->att_data.att_static->att_data.att_uid;
  }
  //if unsuccessful
  return 0;
}

Message** Session::getMessages() {
  this->resetNumMessages();

  mailimap_set* set = mailimap_set_new_interval(1, 0);
  //define a a fetch attribute object (first empty), define a fetch attribute object set
  // to search fo UID, then add to the fetch_obj
  mailimap_fetch_type* fetch_obj = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_fetch_att* uid_fetch_att = mailimap_fetch_att_new_uid();
  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_obj, uid_fetch_att), "couldn't add add uid fetch attribute");

  clist* fetch_result;
  //mailimap_fetch returns an integer code to express success of operation (status code=0)
  //need to catch scenarios when the server mailbox is empty (returns a status code != 0)
  //or unable to fetch from server for other reason
  int status = mailimap_fetch(this->getSession(), set, fetch_obj, &fetch_result);

  if (status == MAILIMAP_NO_ERROR) {
    // get the dynamic num_messages
    for (clistiter* cur = clist_begin(fetch_result) ; cur != NULL ; cur = clist_next(cur)) {
      this->incrementNumMessages();
    }
    //initialize an array of message pointers to NULL. The array has one more space than there are
    //messages, as the last NULL will trigger terminating loops in further functions.
    this->messages = new Message*[this->getNumMessages() + 1 ]{};
    int index = 0;
    for (clistiter* cur = clist_begin(fetch_result) ; cur != NULL ; cur = clist_next(cur)) {
      //static cast of clist void* to mailimap_msg_att* type
      mailimap_msg_att* msg_att = (mailimap_msg_att*) clist_content(cur);

      uint32_t uid = getUID(msg_att);

      //uid 0 if error in retrieving
      if (!uid)
        continue; 

      Message* message = new Message(this, uid);
      message->setMessageAtts();

      this->messages[index] = message;
      index++;
    }
    mailimap_fetch_list_free(fetch_result);

  } else {
    this->messages = new Message*[this->getNumMessages() + 1 ]{};
  }

  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_obj);
  return this->messages;
};

Session::~Session() {
  for (int i = 0 ; i < this->num_messages ; i++ ) {
    delete this->messages[i];
  }
  delete [] this->messages;
  mailimap_logout(this->imap_session);
  mailimap_free(this->imap_session);
};


/*MESSAGE CLASS MEMBER FUNCTION DEFINITIONS*/
Message::Message(Session* session, uint32_t uid) : session(session), uid(uid) {
};

string Message::getBody() {
  return this->body;
};

string Message::getField(string fieldname) {
  if (fieldname == "From")
    return this->from;
  if (fieldname == "Subject")
    return this->subject;

  return "";
};

void Message::setMessageAtts() {
  this->setBody();
  this->setHeaders("subject");
  this->setHeaders("from");
}

void Message::stringParser(string& in_string) {
  string delimiter = ": ";
  in_string = in_string.substr(in_string.find(delimiter) + 2, in_string.length());
  in_string = in_string.substr(0, in_string.length() - 4);
}

void Message::setBody() {
  mailimap_set* set = mailimap_set_new_single(this->uid);
  // define a a fetch attribute object (first empty), define a fetch attribute object set
  //  to search fo UID, then add to the fetch_obj
  mailimap_fetch_type* fetch_obj = mailimap_fetch_type_new_fetch_att_list_empty();
  mailimap_section* section = mailimap_section_new(NULL);
  mailimap_fetch_att* body_fetch_att = mailimap_fetch_att_new_body_section(section);
  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_obj, body_fetch_att), "coudldn't add body fetch attribute");

  clist* fetch_result;
  check_error(mailimap_uid_fetch(this->session->getSession(), set, fetch_obj, &fetch_result), "couldn't fetch");

  for (clistiter* cur_msg = clist_begin(fetch_result) ; cur_msg != NULL ; cur_msg = clist_next(cur_msg)) {
    mailimap_msg_att* msg_att = (mailimap_msg_att*) clist_content(cur_msg);
    for (clistiter* cur_att = clist_begin(msg_att->att_list) ; cur_att != NULL ; cur_att = clist_next(cur_att)) {
      mailimap_msg_att_item* item = (mailimap_msg_att_item*) clist_content(cur_att);

        if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
          if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_BODY_SECTION) {
            this->body = item->att_data.att_static->att_data.att_body_section->sec_body_part;
          }
        }

    }
  }

  mailimap_fetch_list_free(fetch_result);
  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_obj);
};

void Message::setHeaders(string field_str) {
  string received_string;

  clist* hdr_list = clist_new();
  char* fieldname = new char[field_str.length() + 1] {};
  strcpy(fieldname, field_str.c_str());
  clist_append(hdr_list, fieldname);

  mailimap_set* set = mailimap_set_new_single(this->uid);
  mailimap_fetch_type* fetch_obj = mailimap_fetch_type_new_fetch_att_list_empty();
  struct mailimap_header_list* header_list =  mailimap_header_list_new(hdr_list);
  struct mailimap_section* section =  mailimap_section_new_header_fields(header_list);
  mailimap_fetch_att* headers_fetch_att = mailimap_fetch_att_new_body_section(section);

  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_obj, headers_fetch_att), "coudldn't add body fetch attribute");

  clist* fetch_result;
  check_error(mailimap_uid_fetch(this->session->getSession(), set, fetch_obj, &fetch_result), "couldn't fetch");

  for (clistiter* cur_msg = clist_begin(fetch_result) ; cur_msg != NULL ; cur_msg = clist_next(cur_msg)) {
    mailimap_msg_att* msg_att = (mailimap_msg_att*) clist_content(cur_msg);
    for (clistiter* cur_att = clist_begin(msg_att->att_list) ; cur_att != NULL ; cur_att = clist_next(cur_att)) {
      mailimap_msg_att_item* item = (mailimap_msg_att_item*) clist_content(cur_att);
      if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
        if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_BODY_SECTION) {
          received_string = item->att_data.att_static->att_data.att_body_section->sec_body_part;
          stringParser(received_string);
          if (field_str == "subject") {
            this->subject = received_string;
          } else {
            this->from = received_string;
          }
        }
      }
    }

  }
  mailimap_fetch_list_free(fetch_result);
  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_obj);
};


void Message::deleteFromMailbox() {
  //need to protect from crashes when there are no messages in mailbox
  if (this) {
    mailimap_set* set = mailimap_set_new_single(this->uid);
    struct mailimap_flag_list* flag_list = mailimap_flag_list_new_empty();
    struct mailimap_flag* delete_flag = mailimap_flag_new_deleted();
    check_error(mailimap_flag_list_add(flag_list, delete_flag), "couldn't add delete flag");
    struct mailimap_store_att_flags* store_att_flags = mailimap_store_att_flags_new_set_flags(flag_list);
    check_error(mailimap_uid_store(this->session->getSession(), set, store_att_flags), "couldn't change the flags");

    check_error(mailimap_expunge(this->session->getSession()), "couldn't delete");

    mailimap_set_free(set);
    mailimap_store_att_flags_free(store_att_flags);

    for (int i = 0 ; i < this->session->getNumMessages() ; i++) {
      if (this->session->getMessage(i)->uid == this->uid) {
        continue;
      }
      delete this->session->getMessage(i);
    }
    this->session->delMessagesArray();
    this->session->updateUI();
    delete this;
  }
}
