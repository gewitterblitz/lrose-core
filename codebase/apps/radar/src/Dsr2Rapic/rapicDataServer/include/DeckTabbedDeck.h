
//////////////////////////////////////////////////////////////
//
// Header file for DeckTabbedDeck
//
//    This file is generated by RapidApp 1.2
//
//    This class is derived from VkTabbedDeck
//
//    When you modify this header file, limit your changes to those
//    areas between the "//---- Start/End editable code block" markers
//
//    This will allow the builder to integrate changes more easily
//
//    This class is a ViewKit user interface "component".
//    For more information on how components are used, see the
//    "ViewKit Programmers' Manual", and the RapidApp
//    User's Guide.
//////////////////////////////////////////////////////////////
#ifndef DECKTABBEDDECK_H
#define DECKTABBEDDECK_H
#include <Vk/VkTabbedDeck.h>
//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations



//---- DeckTabbedDeck class declaration

class DeckTabbedDeck : public VkTabbedDeck
{

  public:

    DeckTabbedDeck ( const char *, Widget );
    DeckTabbedDeck ( const char * );
    ~DeckTabbedDeck();
    const char *  className();

    static VkComponent *CreateDeckTabbedDeck( const char *name, Widget parent ); 

    //---- Start editable code block: DeckTabbedDeck public


    //---- End editable code block: DeckTabbedDeck public



    protected:


    // Classes created by this class

    class RpCursorDataFormClass *_rpCursorDataFormClassUI;
    class RpSeqSpdDepthFormClass *_rpSeqSpdDepthFormClassUI;

    // Widgets created by this class

    Widget  _deck;


    //---- Start editable code block: DeckTabbedDeck protected


    //---- End editable code block: DeckTabbedDeck protected



  private: 

    // Array of default resources

    static String      _defaultDeckTabbedDeckResources[];

    //---- Start editable code block: DeckTabbedDeck private


    //---- End editable code block: DeckTabbedDeck private
};
//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code

#endif

