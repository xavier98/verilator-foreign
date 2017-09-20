// ../verilator/verilator_bin B1.v --cc

module B1
  (input din1,
   input din2,
   output dout
   );
   
   B2 u_c(din1,din2,dout);
   
endmodule

