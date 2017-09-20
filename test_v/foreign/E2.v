// verilator lint_off UNOPTFLAT
module E2
  (input clk,
   input rst,
   input  din,
   output dout
   );
   // verilator foreign_module
   reg   doutr = 1'b1;
   assign dout = doutr;
   wire [2:0] tmp;
   assign tmp[0] = 1'b1;
   assign tmp[1] = 1'b1;
   assign tmp[2] = 1'b1;
   wire [2:0] out;
   assign out[0] = tmp[0] ^ din;
   assign out[1] = (tmp[1] ^ din) ? out[0] : din;
   assign out[2] = (tmp[2] ^ din) ? out[1] : din;   
   always @(posedge clk or posedge rst)
     doutr <= out[2];
endmodule
