
module A2
  (input din,
   output dout
   );
   // verilator foreign_module
   
   assign dout = ~din;
   
endmodule
