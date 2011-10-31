


void test_codes(int* i, int* n_codes, short4* code, short4* ncode)
{
  short4 codes[8], ncodes[8];
  *n_codes = 8;
  codes[0]=(short4)(0,0,0,0);   ncodes[0]=(short4)(1,0,0,0);
  codes[1]=(short4)(2,0,0,0);   ncodes[1]=(short4)(3,0,0,0);
  codes[2]=(short4)(0,2,0,0);   ncodes[2]=(short4)(0,3,0,0);
  codes[3]=(short4)(2,2,0,0);   ncodes[3]=(short4)(3,3,0,0);
  codes[4]=(short4)(0,0,2,0);   ncodes[4]=(short4)(0,0,3,0);
  codes[5]=(short4)(2,0,2,0);   ncodes[5]=(short4)(3,0,3,0);
  codes[6]=(short4)(0,2,2,0);   ncodes[6]=(short4)(0,3,3,0);
  codes[7]=(short4)(2,2,2,0);   ncodes[7]=(short4)(3,3,3,0);
  (*code) = codes[*i]; (*ncode) = ncodes[*i];
}


__kernel
void
test_traverse(__read_only image2d_t cells, __global float2* cell_data,
              __global int4* results)
{
  short4 code, ncode;
  int n_codes = 0;
  int i= 0;
  //determine the number of codes
  test_codes(&i, &n_codes, &code, &ncode);
  short4 found_loc_code = (short4)(0,0,0,0);
  for (i = 0; i<n_codes; ++i)
  {
    test_codes(&i, &n_codes, &code, &ncode);
    short4 root = (short4)(0,0,0,2);
    int cell_ptr = traverse(cells, 0, root , code, &found_loc_code);
    int4 res = convert_int4(found_loc_code);
    results[2*i]=res;
    res = (int4)cell_ptr;
    results[2*i+1]=res;
  }
}

__kernel
void
test_traverse_to_level(__read_only image2d_t cells, __global float2* cell_data, __global int4* results)
{
  short4 code, ncode;
  int n_codes = 0;
  int i= 0;
  //determine the number of codes
  test_codes(&i, &n_codes, &code, &ncode);
  short4 found_loc_code = (short4)(0,0,0,0);
  for (i = 0; i<n_codes; ++i)
  {
    test_codes(&i, &n_codes, &code, &ncode);
    short4 root = (short4)(0,0,0,2);
    int level = 1;
    int cell_ptr = traverse_to_level(cells, 0, root , code,
                                     level, &found_loc_code);
    int4 res = convert_int4(found_loc_code);
    results[2*i]=res;
    res = (int4)cell_ptr;
    results[2*i+1]=res;
  }
}
__kernel
void
test_traverse_force(__read_only image2d_t cells, __global float2* cell_data,
                    __global int4* results)
{
  int result_ptr = 0;
  short4 found_loc_code = (short4)(0,0,0,0);
  short4 start_code = (short4)(0,0,0,1);

  short4 target_code = (short4)(2,0,0,0);
  int cell_ptr =
    traverse_force(cells, 1, start_code, target_code, &found_loc_code);
  results[result_ptr++]=convert_int4(found_loc_code);
  results[result_ptr++]=(int4)cell_ptr;

  target_code = (short4)(0,2,0,0);
  cell_ptr =
    traverse_force(cells, 1, start_code, target_code, &found_loc_code);
  results[result_ptr++]=convert_int4(found_loc_code);
  results[result_ptr++]=(int4)cell_ptr;

  target_code = (short4)(0,0,2,0);
  cell_ptr =
    traverse_force(cells, 1, start_code, target_code, &found_loc_code);
  results[result_ptr++]=convert_int4(found_loc_code);
  results[result_ptr++]=(int4)cell_ptr;

  target_code = (short4)(2,2,2,0);
  cell_ptr =
    traverse_force(cells, 1, start_code, target_code, &found_loc_code);
  results[result_ptr++]=convert_int4(found_loc_code);
  results[result_ptr++]=(int4)cell_ptr;
}

__kernel
void
test_common_ancestor(__read_only image2d_t cells, __global float2* cell_data,
                     __global int4* results)
{
  short4 code, ncode;
  int n_codes = 0;
  int i= 0;
  //determine the number of codes
  test_codes(&i, &n_codes, &code, &ncode);
  short4 ancestor_loc_code = (short4)(0,0,0,0);
  for (i = 0; i<n_codes; ++i)
  {
    test_codes(&i, &n_codes, &code, &ncode);
    int start_ptr = 9+8*i;
    int ancestor_ptr = common_ancestor(cells, start_ptr,
                                       code, ncode,
                                       &ancestor_loc_code);

    results[2*i] = convert_int4(ancestor_loc_code);
    results[2*i+1] = (int4)ancestor_ptr;
  }
}


__kernel
void
test_neighbor(__read_only image2d_t cells, __global float2* cell_data,
              __global int4* results)
{
  short4 faces[6];
  faces[0]=X_MIN;   faces[1]=X_MAX;
  faces[2]=Y_MIN;   faces[3]=Y_MAX;
  faces[4]=Z_MIN;   faces[5]=Z_MAX;
  int n_levels = 3;
  int cell_ptr = 9;
  int result_ptr = 0;
  // test the lower left cell, which has no neighbors on the min faces
  short4 code = (short4)(0,0,0,0);
  for (int j=0; j<6; ++j) {
    short4 eface = faces[j];
    short4 neighbor_code = (short4)-1;

    int neighbor_ptr = neighbor(cells, cell_ptr, code,
                                eface, n_levels , &neighbor_code);
    results[result_ptr++]=convert_int4(neighbor_code);

    results[result_ptr++]=(int4)neighbor_ptr;
  }
  // test a cell having all neighbors
  code = (short4)(1,1,1,0);
  cell_ptr = 16;
  for (int j=0; j<6; ++j) {
    short4 eface = faces[j];
    short4 neighbor_code = (short4)-1;

    int neighbor_ptr = neighbor(cells, cell_ptr, code,
                                eface, n_levels , &neighbor_code);
    results[result_ptr++]=convert_int4(neighbor_code);

    results[result_ptr++]=(int4)neighbor_ptr;
  }
}