#ifndef vxl_doc_rules_h_
#define vxl_doc_rules_h_
#ifdef __GNUC__
#pragma interface
#endif

//            (This is an example of how to code document your    ) 
//            ( file. Comments with brackets, like this one,      )
//            ( should be read but not copied into your file.     )                       


//            (The style used here is specific to VXL             )
//            ( It is based on doxygen, with modifications.       )
//            ( So rather than use                                )
//            (        /** Brief comment                          )
//            (        */                                         )
//            ( notation, we instead use                          )
//            (         //: Brief comment                         )
//            (The main reason for this is that /* */ comment     )
//            ( markers do not nest, so making it hard to quickly )
//            ( comment out sections of code.                     )
//            (Apart from this, you can use full doxygen notation.)
//            (A simple perl script converts this notation        )
//            ( to full doxygen format on the fly.                )



// This is vxl_doc_rules.h 

//            (The above line indicates the file name             )
//            ( the \file command below tells Doxygen which       )
//            ( file the comments correspond to, so should        )
//            ( be left blank in general to minimise the          )
//            ( risks of mistakes                                 )
//            (Don't forget to leave an empty line after this one.)

//:
// \file
// \brief give a brief description of the file.
// \author Whoever, (erehwon)
// \date 01/01/01
// \author Someone else
// Here you can write the long description of what's in this file
// Doxygen will add this to the extended documentation of the file
// Put the data and author tags immediatly following the brief tag,
// as doxygen (poor thing) is easily confused otherwise.
//
// \verbatim
// Modifications
// IMS (Manchester) 14/03/2001: Tidied up the documentation
// \endverbatim

//: Brief description of the class
// The long description starts here
// \verbatim
// Everything between these tags is not formatted but kept "as is".
// This is similar to the \verbatim command in LaTeX.
// \endverbatim
// The long description ends here
class my_class {
public:

  //: Brief description of method don't forget the colon
  // Do not leave any blank lines with out the //, or doxygen
  // will think you have ended the documentation comment, and
  // started an ordinary code comment.
  //
  // If you want to start a new parapraph, put it a line with just //
  //
  // If you want to do some special stuff such as formulas, have
  // a look at the doxygen documentation http://www.stack.nl/~dimitri/doxygen/
  // Be careful, doxygen is not very tolerant: make sure you test build the
  // documentation after using any of the advanced stuff.
  my_class();

  //: Brief description of the enum.
  // Here comes the detailed description of the enum
  enum vxl_formats {
    VXL_FLOAT,      /*!< This is documentation for this element of the enum */
    VXL_COMPLEX,    /*!< This works for Doxygen only */
    VXL_DOUBLE      /*!< Remark the needed '<' for this ! */
  };

};

#endif // vxl_doc_rules_h_
