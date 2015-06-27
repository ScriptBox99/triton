#include <cmath>
#include "common.hpp"
#include "isaac/array.h"
#include "isaac/wrap/clBLAS.h"

namespace ad = isaac;

template<typename T>
void test_row_wise_reduction(T epsilon, simple_vector_base<T> & cy, simple_matrix_base<T> const & cA, simple_vector_base<T> & cx,
                                        ad::array & y, ad::array const & A, ad::array & x, interface_t interface)
{
  int failure_count = 0;


  ad::int_t M = A.shape()[0];
  ad::int_t N = A.shape()[1];

  simple_vector<T> bufy(M);
  simple_vector<T> bufx(N);

  ad::driver::CommandQueue queue = ad::driver::queues[y.context()][0];

  T yi = 0, xi = 0;
  const char * PREFIX = interface==clBLAS?"[BLAS]":"[C++]";
#define TEST_OPERATION(NAME, SIZE1, SIZE2, REDUCTION, ASSIGNMENT, GPU_REDUCTION, RES, BUF, CRES)\
  std::cout << PREFIX << " " << NAME "..." << std::flush;\
  for(int i = 0 ; i < SIZE1 ; ++i)\
  {\
    yi = 0;\
    xi = 0;\
    for(int j = 0 ; j < SIZE2 ; ++j)\
      REDUCTION;\
    ASSIGNMENT;\
  }\
  GPU_REDUCTION;\
  ad::copy(RES, BUF.data());\
  if(diff(CRES, BUF, epsilon))\
  {\
    failure_count++;\
    std::cout << " [Failure!]" << std::endl;\
  }\
  else\
    std::cout << std::endl;

  ad::int_t offA = A.start()[0] + A.start()[1]*A.ld();

  if(interface==clBLAS)
  {
      cl_command_queue clqueue = (*queue.handle().cl)();

      TEST_OPERATION("GEMV(COL, NoTrans)", M, N, yi+=cA(i,j)*cx[j], cy[i] = yi,
                     BLAS<T>::F(clblasSgemv, clblasDgemv)(clblasColumnMajor, clblasNoTrans, M, N, 1, CHANDLE(A), offA, A.ld()*A.stride()[1],
                                CHANDLE(x), x.start()[0], x.stride()[0], 0, CHANDLE(y), y.start()[0], y.stride()[0],
                                1, &clqueue, 0, NULL, NULL), y, bufy, cy);

      TEST_OPERATION("GEMV(COL, Trans)", N, M, xi+=cA(j,i)*cy[j], cx[i] = xi,
                     BLAS<T>::F(clblasSgemv, clblasDgemv)(clblasColumnMajor, clblasTrans, N, M, 1, CHANDLE(A), offA, A.ld()*A.stride()[1],
                                CHANDLE(y), y.start()[0], y.stride()[0], 0, CHANDLE(x), x.start()[0], x.stride()[0],
                                1, &clqueue, 0, NULL, NULL), x, bufx, cx);
  }
  else
  {
      TEST_OPERATION("y = A.x", M, N, yi+=cA(i,j)*cx[j], cy[i] = yi, y = dot(A,x), y, bufy, cy);
      TEST_OPERATION("x = A'.y", N, M, xi+=cA(j,i)*cy[j], cx[i] = xi, x = dot(trans(A),y), x, bufx, cx);
  }

  if(failure_count>0)
    exit(EXIT_FAILURE);
}

template<typename T>
void test_impl(T epsilon, ad::driver::Context const & ctx)
{
  int_t M = 1324;
  int_t N = 1143;
  int_t SUBM = 184;
  int_t SUBN = 145;

  INIT_VECTOR(M, SUBM, 7, 2, cy, y, ctx);
  INIT_VECTOR(N, SUBN, 5, 3, cx, x, ctx);
  INIT_MATRIX(M, SUBM, 9, 1, N, SUBN, 8, 4, cA, A, ctx);
  INIT_MATRIX(M, SUBM, 9, 5, N, SUBN, 8, 4, cAPP, APP, ctx);


  std::cout << "full..." << std::endl;
  test_row_wise_reduction(epsilon, cy_full, cA_full, cx_full, y_full, A_full, x_full, clBLAS);
  test_row_wise_reduction(epsilon, cy_full, cAPP_full, cx_full, y_full, APP_full, x_full, CPP);
  std::cout << "slice..." << std::endl;
  test_row_wise_reduction(epsilon, cy_slice, cA_slice, cx_slice, y_slice, A_slice, x_slice, clBLAS);
  test_row_wise_reduction(epsilon, cy_slice, cAPP_slice, cx_slice, y_slice, APP_slice, x_slice, CPP);
}

int main()
{
  auto data = ad::driver::queues.contexts();
  for(const auto & elem : data)
  {
    ad::driver::Device device = elem.second[0].device();
    std::cout << "Device: " << device.name() << " on " << device.platform().name() << " " << device.platform().version() << std::endl;
    std::cout << "---" << std::endl;
    std::cout << ">> float" << std::endl;
    test_impl<float>(1e-4, elem.first);
    std::cout << ">> double" << std::endl;
    test_impl<double>(1e-9, elem.first);
    std::cout << "---" << std::endl;
  }
  return EXIT_SUCCESS;
}