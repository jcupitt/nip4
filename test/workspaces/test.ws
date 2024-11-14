<?xml version="1.0"?>
<root xmlns="http://www.vips.ecs.soton.ac.uk/nip/9.0.0">
  <Workspace view="WORKSPACE_MODE_REGULAR" scale="1" offset="0" locked="false" lpane_position="200" lpane_open="false" rpane_position="400" rpane_open="false" local_defs="// private definitions for this tab&#10;" name="tab2" filename="$HOME/GIT/nip4/test/workspaces/test.ws" major="9" minor="0">
    <Column x="5" y="5" open="true" selected="false" sform="false" next="9" name="A">
      <Subcolumn vislevel="3">
        <Row popup="false" name="A1">
          <Rhs vislevel="1" flags="1">
            <iText formula="Scale &quot;hello&quot; 0 100 50"/>
            <Slider/>
            <Subcolumn vislevel="0"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A2">
          <Rhs vislevel="3" flags="7">
            <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
            <Subcolumn vislevel="1"/>
            <iText formula="Hist_new_item.Hist_item.action"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A3">
          <Rhs vislevel="1" flags="1">
            <iImage image_left="0" image_top="0" image_mag="0" show_status="false" show_paintbox="false" show_convert="false" show_rulers="false" scale="0" offset="0" falsecolour="false" type="true"/>
            <Subcolumn vislevel="0"/>
            <iText formula="Image_file &quot;$HOME/pics/vipsdisp-logo-512x512.png&quot;"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A4">
          <Rhs vislevel="1" flags="1">
            <iArrow>
              <iRegiongroup/>
            </iArrow>
            <Subcolumn vislevel="0"/>
            <iText formula="Arrow A3 491 363 (-474) (-225)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A5">
          <Rhs vislevel="3" flags="7">
            <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
            <Subcolumn vislevel="1">
              <Row name="x">
                <Rhs vislevel="0" flags="4">
                  <iText/>
                </Rhs>
              </Row>
              <Row name="super">
                <Rhs vislevel="0" flags="4">
                  <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="width">
                <Rhs vislevel="1" flags="1">
                  <Slider/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="displace">
                <Rhs vislevel="1" flags="1">
                  <Slider caption="Horizontal displace" from="-50" to="50" value="-16.315789473684212"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="vdisplace">
                <Rhs vislevel="1" flags="1">
                  <Slider caption="Vertical displace" from="-50" to="50" value="-18.421052631578949"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
            </Subcolumn>
            <iText formula="Hist_graph_item.action A4"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A6">
          <Rhs vislevel="1" flags="4">
            <iText formula="[1..100]"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A7">
          <Rhs vislevel="3" flags="7">
            <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
            <Subcolumn vislevel="1">
              <Row name="x">
                <Rhs vislevel="3" flags="4">
                  <iText/>
                </Rhs>
              </Row>
              <Row name="super">
                <Rhs vislevel="0" flags="4">
                  <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="caption">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="Chart caption"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="format">
                <Rhs vislevel="1" flags="1">
                  <Option caption="Format" labelsn="3" labels0="YYYY" labels1="XYYY" labels2="XYXY" value="0"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="style">
                <Rhs vislevel="1" flags="1">
                  <Option caption="Style" labelsn="4" labels0="Point" labels1="Line" labels2="Spline" labels3="Bar" value="0"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="auto">
                <Rhs vislevel="1" flags="1">
                  <Toggle caption="Auto Range" value="true"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="xmin">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="X range minimum"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="xmax">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="X range maximum"/>
                  <Subcolumn vislevel="0">
                    <Row name="caption">
                      <Rhs vislevel="0" flags="4">
                        <iText/>
                      </Rhs>
                    </Row>
                    <Row name="expr">
                      <Rhs vislevel="0" flags="4">
                        <iText formula="500"/>
                      </Rhs>
                    </Row>
                    <Row name="super">
                      <Rhs vislevel="1" flags="4">
                        <Subcolumn vislevel="0"/>
                        <iText/>
                      </Rhs>
                    </Row>
                  </Subcolumn>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="ymin">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="Y range minimum"/>
                  <Subcolumn vislevel="0">
                    <Row name="caption">
                      <Rhs vislevel="0" flags="4">
                        <iText/>
                      </Rhs>
                    </Row>
                    <Row name="expr">
                      <Rhs vislevel="0" flags="4">
                        <iText formula="25"/>
                      </Rhs>
                    </Row>
                    <Row name="super">
                      <Rhs vislevel="1" flags="4">
                        <Subcolumn vislevel="0"/>
                        <iText/>
                      </Rhs>
                    </Row>
                  </Subcolumn>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="ymax">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="Y range maximum"/>
                  <Subcolumn vislevel="0">
                    <Row name="caption">
                      <Rhs vislevel="0" flags="4">
                        <iText/>
                      </Rhs>
                    </Row>
                    <Row name="expr">
                      <Rhs vislevel="0" flags="4">
                        <iText formula="255"/>
                      </Rhs>
                    </Row>
                    <Row name="super">
                      <Rhs vislevel="1" flags="4">
                        <Subcolumn vislevel="0"/>
                        <iText/>
                      </Rhs>
                    </Row>
                  </Subcolumn>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="xcaption">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="X axis caption"/>
                  <Subcolumn vislevel="0">
                    <Row name="caption">
                      <Rhs vislevel="0" flags="4">
                        <iText/>
                      </Rhs>
                    </Row>
                    <Row name="expr">
                      <Rhs vislevel="0" flags="4">
                        <iText formula="&quot;Position&quot;"/>
                      </Rhs>
                    </Row>
                    <Row name="super">
                      <Rhs vislevel="1" flags="4">
                        <Subcolumn vislevel="0"/>
                        <iText/>
                      </Rhs>
                    </Row>
                  </Subcolumn>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="ycaption">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="Y axis caption"/>
                  <Subcolumn vislevel="0">
                    <Row name="caption">
                      <Rhs vislevel="0" flags="4">
                        <iText/>
                      </Rhs>
                    </Row>
                    <Row name="expr">
                      <Rhs vislevel="0" flags="4">
                        <iText formula="&quot;Pixel value&quot;"/>
                      </Rhs>
                    </Row>
                    <Row name="super">
                      <Rhs vislevel="1" flags="4">
                        <Subcolumn vislevel="0"/>
                        <iText/>
                      </Rhs>
                    </Row>
                  </Subcolumn>
                  <iText/>
                </Rhs>
              </Row>
              <Row name="series_captions">
                <Rhs vislevel="1" flags="1">
                  <Expression caption="Series captions"/>
                  <Subcolumn vislevel="0"/>
                  <iText/>
                </Rhs>
              </Row>
            </Subcolumn>
            <iText formula="Hist_plot_item.action A5"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A8">
          <Rhs vislevel="5" flags="7">
            <Plot plot_left="0" plot_top="0" plot_mag="100" show_status="false"/>
            <Subcolumn vislevel="3"/>
            <iText formula="A7 ++ (A7 * 4) ++ (A7 * 8)"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
    <Column x="676" y="5" open="true" selected="true" sform="false" next="2" name="B">
      <Subcolumn vislevel="3">
        <Row popup="false" name="B1">
          <Rhs vislevel="3" flags="7">
            <Matrix/>
            <Subcolumn vislevel="1"/>
            <iText formula="Matrix_build_item.Matrix_laplacian_item.action"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
  </Workspace>
</root>
