#include <testlib/testlib_test.h>
//:
// \file
#include <vcl_iostream.h>
#include <boxm/boxm_scene_parser.h>
#include <boxm/ocl/boxm_ocl_scene.h>
#include <boxm/ocl/tests/open_cl_test_data.h>


bool test_ocl_scene_parser()
{
  boxm_scene_parser parser;
  vcl_string filename = "small_block_scene.xml";
  if (filename.size() > 0) {
    vcl_FILE* xmlFile = vcl_fopen(filename.c_str(), "r");
    if (!xmlFile){
    vcl_cerr << filename.c_str() << " error on opening\n";
    return 0;
    }
    if (!parser.parseFile(xmlFile)) {
      vcl_cerr << XML_ErrorString(parser.XML_GetErrorCode()) << " at line "
               << parser.XML_GetCurrentLineNumber() << '\n';
      return false;
    }
  }

  vcl_string path, block_pref;
  parser.paths(path, block_pref);
  vcl_cout<<"path: "<<path<<'\n'
          <<"block pref: "<<block_pref<<'\n'
          <<"origin: "<<parser.origin()<<'\n'
          <<"block dim: "<<parser.block_dim()<<'\n'
          <<"Block nums: "<<parser.block_nums()<<'\n'
          <<"ap model: "<<parser.app_model()<<'\n'
          <<"multi_bin: "<<parser.multi_bin()<<'\n'
          <<"save internal: "<<parser.save_internal_nodes()<<'\n'
          <<"save platform ind: "<<parser.save_platform_independent()<<'\n'
          <<"load all blocks: "<<parser.load_all_blocks()<<vcl_endl;
  int num_buffs, buff_size;
  parser.tree_buffer_shape(num_buffs, buff_size);
  vcl_cout<<"Init num tree buffs: "<<num_buffs<<'\n'
          <<"Init buffer size : " << buff_size << vcl_endl;
  return true;
}


bool test_ocl_scene_write()
{
  vcl_cout<<vcl_endl<<"test_ocl small block scene write"<<vcl_endl;

  //establish directory with .xml file
  vcl_string dir = "./";
  vcl_string block_path = dir + "blocks.bin";
  vcl_string data_path = dir + "data.bin";

  //remove the .bin files,
  vcl_remove(block_path.c_str());
  vcl_remove(data_path.c_str());

  //initialize a new scene from scratch
  vcl_string filename = dir + "small_block_scene.xml";
  boxm_ocl_scene scene(filename);
  vcl_cout<<scene<<vcl_endl;

  return scene.save_scene(dir);
}

bool test_ocl_scene_read()
{
  vcl_cout<<vcl_endl<<"test_ocl small block scene read"<<vcl_endl;

  //establish .xml file directory
  vcl_string dir = "./";
  vcl_string filename = dir + "small_block_scene.xml";

  //load scene from file
  boxm_ocl_scene scene(filename);
  vcl_cout<<scene<<vcl_endl;

  return true;
}

static void test_scene_io()
{
  vcl_cout<<"TESTING SMALL BLOCK SCENE IO"<<vcl_endl;

  bool parse = test_ocl_scene_parser();
  bool write = test_ocl_scene_write();
  bool read = test_ocl_scene_read();
  TEST("Testing boxm_ocl_scene parser on small block scene ", parse, true);
  TEST("testing boxm_ocl_scene IO write ", write , true);
  TEST("testing boxm_ocl_scene IO read ", read, true);
}

TESTMAIN(test_scene_io);