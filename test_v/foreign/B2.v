module B2
  (input din1,
   input din2,
   output dout
   );
   // verilator foreign_module

   wire [1:0] tmp = {din1 ^ din2, (din1 ^ din2) | din1};
   reg [2:0]  mem;
   assign dout = mem[tmp];

endmodule
