#ifndef IMAP_H
#define IMAP_H
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <functional>

namespace IMAP {

class Message;

/*
Session Class
- handles mailimap server connection
- responsible for deleting message objects
*/
class Session {
private:
	mailimap* imap_session;
	std::string mailbox;
	Message** messages = nullptr;
	int num_messages = 0;

public:
	/*
	Constructor
		sets new imap_session, empty mailbox string, and updateUI lambda function
		-	updateUI lambda function, allows Session to update the UI after changes
	*/
	Session(std::function<void()> updateUI);

	/*
	updateUI
		lambda function used to call refreshMailList in UI
	*/
	std::function<void()> updateUI;

	/*
		getNumMessages()
		helper function, gives access to read num_messages class member
		-	returns this->num_messages
	*/
	int getNumMessages() {return this->num_messages ;}

	/*
		resetNumMessages()
		helper function, sets this->num_messages back to 0. Needed at the beginning
		of the getMessages() process
	*/
	int resetNumMessages() {this->num_messages = 0;}

	/*
		incrementNumMessages()
		helper function, increments this->num_messages by 1
	*/
	void incrementNumMessages() {this->num_messages++ ;}

	/*
		getMessage()
		helper function, gives access to a message object in this->messages array
		-	i: the index of the message to be retrieved
		-	returns a Message object
	*/
	Message* getMessage(int i) {return this->messages[i] ;}

	/*
		getSession()
		helper function, gives access to the mailimap* object, this->imap_session which
		is requireed by Message objects
		-	returns this->imap_session
	*/
	mailimap* getSession() {return this->imap_session ;}
	/*
		getMessages()
	 	Get all messages in the INBOX mailbox terminated by a nullptr (like done in class)
		set to this->messages
		- returns pointer to array of Message pointers
	*/
	Message** getMessages();

	/*
		getUID()
		helper function for getMessages, extracts UID dat out of mailimap_msg_att object
		-	returns server message UID
	*/
	uint32_t getUID(struct mailimap_msg_att* msg_att);

	/*
		connect()
	 	connect to the specified server (143 is the standard unencrypte imap port)
	*/
	void connect(std::string const& server, size_t port = 143);

	/*
		login()
	 	log in to the server (connect first, then log in)
	*/
	void login(std::string const& userid, std::string const& password);

	/*
		selectMailbox()
	 	select a mailbox (only one can be selected at any given time)
	 	this can only be performed after login
	*/
	void selectMailbox(std::string const& mailbox);

	/*
	delMessagesArray()
	helper function, deletes this->messages array. Required when the mailbox is
	refreshed, so it can be reinitialised and repopulated
	*/
	void delMessagesArray() {delete [] this->messages ;}

	/*
		Destructor
		handles deleting of Message objects, and this->messages array
		logsout of imap_session, and deletes this->imap_session
	*/
	~Session();
};

class Message {
	uint32_t uid;
	Session* session;
	std::string from = "";
	std::string subject = "";
	std::string body = "";

public:
	/*
		Constructor
		set the unique identifier: uid and attach a Session obj for access to the
		imap_session
	*/
	Message(Session*, uint32_t);

	/*
		setMessageAtts()
		sets message body, subject and from attributes by calling helper functions
		setBody() and setHeaders()
	*/
	void setMessageAtts();

	/*
		stringParser()
		parses string received from the server to an appropriate format - removing
		special characters and unneccessary prefixes
		- in_string: reference string to be parsed
	*/
	void stringParser(std::string& in_string);

	/*
		setBody()
		retrieves body data from the server and sets to this->body
	*/
	void setBody();

	/*
		setHeaders()
		retrieves header data from the server and sets to appropriate attribute
		this->subject or this->body
		-	field_str: type of header, either "from" or "subject"
	*/
	void setHeaders(std::string field_str);

	/*
		getUID()
		access this->uid
	*/
	int getUID() {return this->uid;}

	/*
		getBody()
		access this->body
	*/
	std::string getBody();

	/*
		getField()
		access this->subject or  this->from depending on input string
		-	fieldname: header requested, either "From" or "subject"
	*/
	std::string getField(std::string fieldname);

	/*
		deleteFromMailbox()
		delete this message and inform the server. Proceeds to clean up all current message
		objects and messages array, and refresh the UI. (In doing so, still exisitng
		messages are redownloaded and reinitialised as Message objects)
	*/
	void deleteFromMailbox();

};


}

#endif /* IMAP_H */
