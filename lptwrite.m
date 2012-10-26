% lptwrite() - Send parallel port TTL trigger
%
% Usage:
%   >> lptwrite(port, value)
%   >> lptwrite(port, value, pulseWidth)
%
% Inputs:
%   port          - double port number
%   value         - double scalar trigger value
%
% Optional inputs:
%   pulseWidth    - double TTL pulse width in microseconds {default: 1000}
%
% Examples:
%   ppdev_mex('Open', 1);
%   lptwrite(1, 255)
%   ppdev_mex('Close', 1);
%
% Author: Andreas Widmann, University of Leipzig, 2012

%123456789012345678901234567890123456789012345678901234567890123456789012

% Copyright (C) 2012 Andreas Widmann, University of Leipzig, widmann@uni-leipzig.de
%
% This program is free software; you can redistribute it and/or modify it
% under the terms of the GNU General Public License as published by the
% Free Software Foundation; either version 2 of the License, or (at your
% option) any later version.
%
% This program is distributed in the hope that it will be useful, but
% WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
% Public License for more details.
%
% You should have received a copy of the GNU General Public License along
% with this program; if not, write to the Free Software Foundation, Inc.,
% 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA

function lptwrite(port, portValue, pulseWidth)

if nargin < 3 || isempty(pulseWidth)
    pulseWidth = 1000;
end
pulseWidth = pulseWidth / 1e6; % was microseconds
if nargin < 2
    error('Not enough input arguments.')
end

ppdev_mex('Write', port, portValue);
WaitSecs(pulseWidth);
ppdev_mex('Write', port, 0);

end

