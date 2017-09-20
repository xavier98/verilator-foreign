module C2
  (input clk,
   input din,
   output dout
   );
   // verilator foreign_module
   reg   doutr;
   assign dout = doutr;
   always @(posedge clk)
     doutr <= din;
endmodule

   
